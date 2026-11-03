#include "recognition.h"
#include "recognition_audio.h"
#include "network.h"
#include "mic_pcm.h"
#include "service_errors.h"
#include "lrclib_client.h"
#include "lyrics.h"
#include "song_cache.h"
#include "lyric_sync.h"
#include "bsp/esp32_s3_touch_amoled_1_75.h"
#include "esp_codec_dev.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nvs.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Experimental manual song ID only: no always-on microphone, logging of
 * audio or credentials, background scans, or web route to export raw PCM.
 * Two physical microphones stay separate as 16-bit stereo in an uncompressed
 * WAV. Only explicit Start authorizes HTTPS transfer to AudD; stop cancels. */
#define LF_RECOG_TOKEN_NS "lf_recog"
#define LF_RECOG_TOKEN_KEY "api_token"
#define LF_RECOG_BOUNDARY "----LyricFlashScan2026"
#define LF_RECOG_MIN_FRAMES (LF_SCAN_RATE*6U)
#define LF_RECOG_RESPONSE_CAP 4096
/* 10-second microphone capture is unaffected. Limit the subsequent HTTPS
 * upload/recognition to a real total budget, not 15s multiplied by every
 * 4096-byte write of an ~1MB WAV. */
#define LF_RECOG_UPLOAD_BUDGET_MS 60000ULL
static uint64_t lf_recog_now_ms(void)
{
    return (uint64_t)(esp_timer_get_time()/1000LL);
}
static atomic_int active_state=ATOMIC_VAR_INIT(LF_RECOG_IDLE);
static atomic_bool stop_flag;
static atomic_uint attempts;
static atomic_bool worker_active;
static lf_lyrics_t *lyrics_cache; /* PSRAM-backed LRC, guarded by result_mux */
static portMUX_TYPE result_mux=portMUX_INITIALIZER_UNLOCKED;
static lf_recog_snapshot_t result;

static void clear_secret(void *ptr,size_t count)
{
    volatile unsigned char *p=(volatile unsigned char *)ptr;
    while(count--)*p++=0;
}
static void status(lf_recog_state_t state,const char *message)
{
    portENTER_CRITICAL(&result_mux);
    result.state=state;
    result.stage_started_ms=lf_recog_now_ms();
    if(message)snprintf(result.message,sizeof result.message,"%s",message);
    portEXIT_CRITICAL(&result_mux);
    atomic_store(&active_state,state);
}
/* Feed only a bounded scalar envelope to the screen, not PCM. The actual
 * explicit AudD recording remains unchanged and never runs in the UI task. */
static void set_count(uint32_t frames,const int16_t *tdm)
{
    uint32_t total=0;
    for(size_t i=0;i<LF_SCAN_TDM_SAMPLES;i+=4){
        int32_t a=tdm[i],b=tdm[i+2];
        total+=(uint32_t)(a<0?-a:a);
        total+=(uint32_t)(b<0?-b:b);
    }
    const uint32_t average=total/(2*LF_SCAN_TDM_FRAMES);
    const uint8_t level=(uint8_t)(average/14U>100U?100U:average/14U);
    portENTER_CRITICAL(&result_mux);
    result.captured_frames=frames;
    result.capture_seconds=frames/LF_SCAN_RATE;
    result.capture_visual_level=level;
    portEXIT_CRITICAL(&result_mux);
}
bool lf_recog_busy(void)
{
    return atomic_load(&worker_active);
}
const char *lf_recog_state_name(lf_recog_state_t state)
{
    switch(state){
    case LF_RECOG_IDLE:return "idle";
    case LF_RECOG_CAPTURE:return "capturing";
    case LF_RECOG_UPLOAD:return "identifying";
    case LF_RECOG_LYRICS:return "finding_lyrics";
    case LF_RECOG_MATCH:return "matched";
    case LF_RECOG_NO_MATCH:return "no_match";
    case LF_RECOG_ERROR:return "error";
    case LF_RECOG_STOPPED:return "stopped";
    default:return "unknown";
    }
}
void lf_recog_snapshot(lf_recog_snapshot_t *out)
{
    if(!out)return;
    portENTER_CRITICAL(&result_mux);
    *out=result;
    portEXIT_CRITICAL(&result_mux);
}
/* Copy one line while the shared PSRAM cache is locked; no lifetime escape. */
bool lf_recog_lyric_line(uint32_t index, lf_lyric_line_t *out)
{
    if(!out)return false;
    bool found=false;
    portENTER_CRITICAL(&result_mux);
    if(lyrics_cache && index<lyrics_cache->count) {
        *out=lyrics_cache->line[index];
        found=true;
    }
    portEXIT_CRITICAL(&result_mux);
    return found;
}
static bool cancelled(void){return atomic_load(&stop_flag);}
static void publish_capture_metrics(const lf_scan_audio_metrics_t *m)
{
    const uint32_t r1=lf_scan_audio_rms(m->mic1_sum_sq,m->frames);
    const uint32_t r2=lf_scan_audio_rms(m->mic2_sum_sq,m->frames);
    const uint32_t clean1=lf_scan_audio_rms(m->mic1_below_rail_sq,
                                             m->mic1_below_rail_frames);
    const uint32_t clean2=lf_scan_audio_rms(m->mic2_below_rail_sq,
                                             m->mic2_below_rail_frames);
    portENTER_CRITICAL(&result_mux);
    result.audio_measured_frames=m->frames;
    result.mic1_peak=m->mic1_peak;result.mic2_peak=m->mic2_peak;
    result.mic1_rms=r1;result.mic2_rms=r2;
    result.mic1_rms_without_rail=clean1;result.mic2_rms_without_rail=clean2;
    result.mic1_near_rail=m->mic1_near_rail;
    result.mic2_near_rail=m->mic2_near_rail;
    result.mic1_half_scale=m->mic1_half_scale;
    result.mic2_half_scale=m->mic2_half_scale;
    result.mic1_rail_bursts=m->mic1_rail_bursts;
    result.mic2_rail_bursts=m->mic2_rail_bursts;
    result.mic1_longest_rail_burst=m->mic1_longest_burst;
    result.mic2_longest_rail_burst=m->mic2_longest_burst;
    result.both_near_rail_frames=m->both_near_rail;
    portEXIT_CRITICAL(&result_mux);
}
static void set_transport_error(int code)
{
    portENTER_CRITICAL(&result_mux);
    result.audd_transport_error_code=code;
    portEXIT_CRITICAL(&result_mux);
}
static void lookup_lyrics_after_match(void)
{
    lf_recog_snapshot_t song={0};
    lf_recog_snapshot(&song);
    if(song.state!=LF_RECOG_MATCH || cancelled())return;
    status(LF_RECOG_LYRICS,"Identified. Checking for synchronized lyrics...");
    /* A 160-line parsed LRC can be ~31KB; keep it out of internal DMA RAM. */
    lf_lyrics_t *parsed=(lf_lyrics_t *)heap_caps_calloc(
        1,sizeof(lf_lyrics_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!parsed){
        portENTER_CRITICAL(&result_mux);
        snprintf(result.lyrics_state,sizeof result.lyrics_state,"unavailable");
        portEXIT_CRITICAL(&result_mux);
        if(!cancelled())status(LF_RECOG_MATCH,"Song identified; lyrics memory unavailable");
        return;
    }
    /* Song identification still comes from the explicit AudD scan.
       Cache hit skips only the LRCLIB HTTPS lookup, never the microphone. */
    bool from_local=lf_song_cache_load_lyrics(song.title,song.artist,parsed);
    const lf_lrclib_state_t found=from_local?LF_LRCLIB_SYNCED:
        lf_lrclib_lookup(song.title,song.artist,parsed,cancelled);
    if(found==LF_LRCLIB_SYNCED && parsed->count && !from_local && !cancelled())
        (void)lf_song_cache_store_lyrics(song.title,song.artist,parsed);
    if(cancelled()){heap_caps_free(parsed);return;}
    portENTER_CRITICAL(&result_mux);
    snprintf(result.lyrics_state,sizeof result.lyrics_state,"%s",
             lf_lrclib_state_name(found));
    if(found==LF_LRCLIB_SYNCED && parsed->count>0) {
        lyrics_cache=parsed;  /* cache was cleared at the start of this scan */
        result.lyric_lines=(uint32_t)parsed->count;
        parsed=NULL;
    }
    portEXIT_CRITICAL(&result_mux);
    if(parsed)heap_caps_free(parsed);
    if(cancelled())return;
    switch(found){
        case LF_LRCLIB_SYNCED:
            status(LF_RECOG_MATCH,from_local?
                   "Song identified; cached synchronized lyrics loaded":
                   "Song identified; synchronized lyrics found");break;
        case LF_LRCLIB_PLAIN:
            status(LF_RECOG_MATCH,"Song identified; only unsynchronized lyrics available");break;
        case LF_LRCLIB_INSTRUMENTAL:
            status(LF_RECOG_MATCH,"Instrumental track; no lyrics");break;
        case LF_LRCLIB_NOT_FOUND:
            status(LF_RECOG_MATCH,"Song identified; lyrics not found");break;
        case LF_LRCLIB_RATE_LIMIT:
            status(LF_RECOG_MATCH,"Song identified; lyrics service busy. Try later");break;
        default:
            status(LF_RECOG_MATCH,"Song identified; lyrics temporarily unavailable");break;
    }
}
static bool load_token(char out[128])
{
    out[0]=0;
    nvs_handle_t h;
    if(nvs_open(LF_RECOG_TOKEN_NS,NVS_READONLY,&h)!=ESP_OK)return false;
    size_t size=128;
    const esp_err_t rc=nvs_get_str(h,LF_RECOG_TOKEN_KEY,out,&size);
    nvs_close(h);
    if(rc!=ESP_OK || !lf_scan_token_valid(out)){
        clear_secret(out,128);return false;
    }
    return true;
}
bool lf_recog_has_token(void)
{
    char token[128]={0};
    const bool ok=load_token(token);
    clear_secret(token,sizeof token);
    return ok;
}
bool lf_recog_save_token(const char *token)
{
    if(lf_recog_busy()||!lf_scan_token_valid(token))return false;
    nvs_handle_t h;
    if(nvs_open(LF_RECOG_TOKEN_NS,NVS_READWRITE,&h)!=ESP_OK)return false;
    esp_err_t err=nvs_set_str(h,LF_RECOG_TOKEN_KEY,token);
    if(err==ESP_OK)err=nvs_commit(h);
    nvs_close(h);
    return err==ESP_OK;
}
bool lf_recog_clear_token(void)
{
    if(lf_recog_busy())return false;
    nvs_handle_t h;
    if(nvs_open(LF_RECOG_TOKEN_NS,NVS_READWRITE,&h)!=ESP_OK)return false;
    esp_err_t err=nvs_erase_key(h,LF_RECOG_TOKEN_KEY);
    if(err==ESP_ERR_NVS_NOT_FOUND)err=ESP_OK;
    if(err==ESP_OK)err=nvs_commit(h);
    nvs_close(h);
    return err==ESP_OK;
}
static bool post_write(esp_http_client_handle_t client,const uint8_t *src,size_t len,
                       uint64_t deadline_ms)
{
    size_t sent=0;
    while(sent<len && !atomic_load(&stop_flag)){
        if(lf_recog_now_ms()>=deadline_ms){
            set_transport_error(ESP_ERR_TIMEOUT);
            return false;
        }
        const size_t chunk=len-sent>4096U?4096U:len-sent;
        const int written=esp_http_client_write(client,(const char *)src+sent,(int)chunk);
        if(written<=0)return false;
        sent+=(size_t)written;
        portENTER_CRITICAL(&result_mux);
        result.upload_bytes_sent+=(uint32_t)written;
        result.upload_progress_ms=lf_recog_now_ms();
        portEXIT_CRITICAL(&result_mux);
    }
    return sent==len;
}
static void parse_recognition(int http_code,const char *json)
{
    cJSON *root=cJSON_Parse(json);
    const cJSON *status_field=root?cJSON_GetObjectItemCaseSensitive(root,"status"):NULL;
    const cJSON *song=root?cJSON_GetObjectItemCaseSensitive(root,"result"):NULL;
    const cJSON *err=root?cJSON_GetObjectItemCaseSensitive(root,"error"):NULL;
    const cJSON *code=err?cJSON_GetObjectItemCaseSensitive(err,"error_code"):NULL;
    int provider_code=cJSON_IsNumber(code)?code->valueint:0;
    if(provider_code==0 && cJSON_IsString(code)){
        char *end=NULL;
        const long parsed=strtol(code->valuestring,&end,10);
        if(end!=code->valuestring && end && *end==0 && parsed>0 && parsed<10000)
            provider_code=(int)parsed;
    }
    const bool success=cJSON_IsString(status_field)&&
        strcmp(status_field->valuestring,"success")==0;
    const cJSON *title=cJSON_IsObject(song)?cJSON_GetObjectItemCaseSensitive(song,"title"):NULL;
    const cJSON *artist=cJSON_IsObject(song)?cJSON_GetObjectItemCaseSensitive(song,"artist"):NULL;
    const cJSON *timecode=cJSON_IsObject(song)?cJSON_GetObjectItemCaseSensitive(song,"timecode"):NULL;
    uint32_t position_ms=0;
    const bool has_timecode=cJSON_IsString(timecode)&&
        lf_sync_parse_timecode_ms(timecode->valuestring,&position_ms);
    const bool has_match=success&&cJSON_IsString(title)&&cJSON_IsString(artist)&&
        title->valuestring[0]&&artist->valuestring[0];
    const lf_service_error_t mapped=lf_recognition_status(http_code,provider_code,has_match);
    portENTER_CRITICAL(&result_mux);
    result.http_status=http_code;
    result.audd_api_error_code=provider_code;
    if(has_match){
        snprintf(result.title,sizeof result.title,"%s",title->valuestring);
        snprintf(result.artist,sizeof result.artist,"%s",artist->valuestring);
        result.audd_timecode_valid=has_timecode;
        result.audd_timecode_ms=has_timecode?position_ms:0;
    }
    portEXIT_CRITICAL(&result_mux);
    if(has_match)status(LF_RECOG_MATCH,"Song identified");
    else if(success && cJSON_IsNull(song))status(LF_RECOG_NO_MATCH,"No matching song found");
    else if(!success && mapped==LF_API_NO_MATCH)status(LF_RECOG_ERROR,"Recognition service returned an error");
    else if(mapped==LF_API_NO_MATCH)status(LF_RECOG_NO_MATCH,"No matching song found");
    else status(LF_RECOG_ERROR,lf_service_error_text(mapped));
    cJSON_Delete(root);
}
static bool send_audio(const uint8_t *wav,size_t wav_bytes,const char *token)
{
    /* This is the only egress for microphone audio, and runs ONLY after an
     * explicit scan. TLS certificate validation is mandatory. */
    char prefix[384];
    const int n=snprintf(prefix,sizeof prefix,
         "--" LF_RECOG_BOUNDARY "\r\n"
         "Content-Disposition: form-data; name=\"api_token\"\r\n\r\n"
         "%s\r\n--" LF_RECOG_BOUNDARY "\r\n"
         "Content-Disposition: form-data; name=\"file\"; filename=\"scan.wav\"\r\n"
         "Content-Type: audio/wav\r\n\r\n",token);
    const char suffix[]="\r\n--" LF_RECOG_BOUNDARY "--\r\n";
    if(n<=0||(size_t)n>=sizeof prefix)return false;
    const size_t body_len=(size_t)n+wav_bytes+sizeof suffix-1U;
    const uint64_t deadline_ms=lf_recog_now_ms()+LF_RECOG_UPLOAD_BUDGET_MS;
    portENTER_CRITICAL(&result_mux);
    result.upload_bytes_sent=0;
    result.upload_bytes_total=(uint32_t)body_len;
    result.upload_progress_ms=lf_recog_now_ms();
    portEXIT_CRITICAL(&result_mux);
    esp_http_client_config_t config={
        .url="https://api.audd.io/",
        .crt_bundle_attach=esp_crt_bundle_attach,
        .timeout_ms=15000,
        .buffer_size=4096,
        .keep_alive_enable=false
    };
    esp_http_client_handle_t client=esp_http_client_init(&config);
    if(!client){set_transport_error(ESP_ERR_NO_MEM);return false;}
    esp_http_client_set_method(client,HTTP_METHOD_POST);
    esp_http_client_set_header(client,"Content-Type",
          "multipart/form-data; boundary=" LF_RECOG_BOUNDARY);
    bool ok=false;
    const esp_err_t open_rc=esp_http_client_open(client,(int)body_len);
    if(open_rc!=ESP_OK)set_transport_error((int)open_rc);
    if(open_rc==ESP_OK){
        if(post_write(client,(const uint8_t*)prefix,(size_t)n,deadline_ms)&&
           post_write(client,wav,wav_bytes,deadline_ms)&&
           post_write(client,(const uint8_t*)suffix,sizeof suffix-1U,deadline_ms)&&
           lf_recog_now_ms()<deadline_ms &&
           !atomic_load(&stop_flag)){
            const int64_t header_len=esp_http_client_fetch_headers(client);
            portENTER_CRITICAL(&result_mux);
            result.http_status=esp_http_client_get_status_code(client);
            portEXIT_CRITICAL(&result_mux);
            if(header_len>=0 && header_len<LF_RECOG_RESPONSE_CAP){
                char *reply=calloc(1,LF_RECOG_RESPONSE_CAP);
                if(reply){
                    const int read=esp_http_client_read_response(client,reply,LF_RECOG_RESPONSE_CAP-1);
                    if(read>0 && read<LF_RECOG_RESPONSE_CAP && !atomic_load(&stop_flag)){
                        reply[read]=0;
                        parse_recognition(esp_http_client_get_status_code(client),reply);
                        ok=true;
                    }
                    clear_secret(reply,LF_RECOG_RESPONSE_CAP);
                    free(reply);
                }
            }
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    clear_secret(prefix,sizeof prefix);
    return ok;
}
static void worker(void *arg)
{
    (void)arg;
    char token[128]={0};
    uint8_t *wav=NULL;
    esp_codec_dev_handle_t codec=NULL;
    bool opened=false,ok=false;
    uint32_t frames=0;
    lf_scan_audio_metrics_t metrics={0};
    bool metrics_published=false;
    if(!load_token(token)){status(LF_RECOG_ERROR,"Set an AudD token in Device Settings");goto done;}
    if(!lf_network_is_connected()){status(LF_RECOG_ERROR,"Connect to Wi-Fi to identify music");goto done;}
    wav=(uint8_t*)heap_caps_malloc(LF_SCAN_WAV_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!wav){status(LF_RECOG_ERROR,"Audio buffer unavailable");goto done;}
    codec=bsp_audio_codec_microphone_init();
    if(!codec){status(LF_RECOG_ERROR,"Microphone controller unavailable");goto done;}
    esp_codec_dev_sample_info_t fs={.sample_rate=(int)LF_SCAN_RATE,
        .channel=4,.channel_mask=0x0fU,.bits_per_sample=16};
    if(esp_codec_dev_open(codec,&fs)!=ESP_CODEC_DEV_OK){
        status(LF_RECOG_ERROR,"Microphone could not open");goto done;
    }
    opened=true;
    if(esp_codec_dev_set_in_channel_gain(codec,0x03,24.0f)!=ESP_CODEC_DEV_OK){
        status(LF_RECOG_ERROR,"Microphone gain setup failed");goto done;
    }
    const int64_t start=esp_timer_get_time();
    portENTER_CRITICAL(&result_mux);
    result.capture_start_ms=(uint64_t)(start/1000LL);
    portEXIT_CRITICAL(&result_mux);
    while(!atomic_load(&stop_flag)&&frames+LF_SCAN_TDM_FRAMES<=LF_SCAN_MAX_FRAMES &&
          esp_timer_get_time()-start<(int64_t)LF_SCAN_SECONDS*1000000LL){
        int16_t tdm[LF_SCAN_TDM_SAMPLES]={0};
        if(esp_codec_dev_read(codec,tdm,sizeof tdm)!=ESP_CODEC_DEV_OK){
            status(LF_RECOG_ERROR,"Microphone recording interrupted");goto done;
        }
        const size_t at=LF_SCAN_WAV_HEADER+frames*4U;
        if(!lf_scan_tdm_to_stereo(wav+at,LF_SCAN_WAV_BYTES-at,tdm,LF_SCAN_TDM_SAMPLES)){
            status(LF_RECOG_ERROR,"Audio conversion failed");goto done;
        }
        lf_scan_audio_metrics_add(&metrics,tdm,LF_SCAN_TDM_SAMPLES);
        frames+=LF_SCAN_TDM_FRAMES;
        set_count(frames,tdm);
    }
    (void)esp_codec_dev_close(codec);opened=false;
    portENTER_CRITICAL(&result_mux);
    result.capture_visual_level=0; /* no level after manual capture ends */
    portEXIT_CRITICAL(&result_mux);
    publish_capture_metrics(&metrics);metrics_published=true;
    if(atomic_load(&stop_flag))goto done;
    if(frames<LF_RECOG_MIN_FRAMES){
        status(LF_RECOG_ERROR,"Not enough recorded audio to identify song");goto done;
    }
    lf_scan_wav_header(wav,frames);
    status(LF_RECOG_UPLOAD,"Identifying song securely with AudD...");
    ok=send_audio(wav,LF_SCAN_WAV_HEADER+(size_t)frames*4U,token);
    if(!ok&&!atomic_load(&stop_flag)){
        lf_recog_snapshot_t last={0};
        lf_recog_snapshot(&last);
        status(LF_RECOG_ERROR,
            last.audd_transport_error_code==ESP_ERR_TIMEOUT?
            "Recognition upload timed out. Tap Play to retry.":
            "Music recognition connection failed. Tap Play to retry.");
    }
    /* The private WAV is no longer needed once AudD has responded. Wipe it
     * BEFORE any additional network call so the lyrics lookup is lightweight. */
    if(wav){clear_secret(wav,LF_SCAN_WAV_BYTES);heap_caps_free(wav);wav=NULL;}
    clear_secret(token,sizeof token);
    if(ok && !atomic_load(&stop_flag))lookup_lyrics_after_match();
done:
    if(!metrics_published)publish_capture_metrics(&metrics);
    if(opened)(void)esp_codec_dev_close(codec);
    if(wav){clear_secret(wav,LF_SCAN_WAV_BYTES);heap_caps_free(wav);}
    clear_secret(token,sizeof token);
    if(atomic_load(&stop_flag))status(LF_RECOG_STOPPED,"Recognition stopped");
    atomic_store(&worker_active,false);
    vTaskDelete(NULL);
}
bool lf_recog_start(void)
{
    lf_pcm_snapshot_t pcm={0};
    lf_pcm_snapshot(&pcm);
    if(pcm.state==LF_PCM_RUNNING)return false;
    if(!lf_recog_has_token()||!lf_network_is_connected())return false;
    bool expected=false;
    if(!atomic_compare_exchange_strong(&worker_active,&expected,true))return false;
    atomic_store(&active_state,LF_RECOG_CAPTURE);
    atomic_store(&stop_flag,false);
    lf_lyrics_t *previous=NULL;
    portENTER_CRITICAL(&result_mux);
    previous=lyrics_cache;lyrics_cache=NULL;
    memset(&result,0,sizeof result);
    snprintf(result.lyrics_state,sizeof result.lyrics_state,"not_checked");
    result.state=LF_RECOG_CAPTURE;
    result.stage_started_ms=lf_recog_now_ms();
    result.attempts=atomic_fetch_add(&attempts,1)+1;
    snprintf(result.message,sizeof result.message,"Recording up to 10 seconds");
    portEXIT_CRITICAL(&result_mux);
    if(previous)heap_caps_free(previous);
    if(xTaskCreate(worker,"lf_song_recog",14336,NULL,tskIDLE_PRIORITY+1,NULL)!=pdPASS){
        status(LF_RECOG_ERROR,"Cannot start recognition task");
        atomic_store(&worker_active,false);
        return false;
    }
    return true;
}
void lf_recog_stop(void){atomic_store(&stop_flag,true);}
