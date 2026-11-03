#include "recognition.h"
#include "recognition_audio.h"
#include "network.h"
#include "mic_pcm.h"
#include "service_errors.h"
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
static atomic_int active_state=ATOMIC_VAR_INIT(LF_RECOG_IDLE);
static atomic_bool stop_flag;
static atomic_uint attempts;
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
    if(message)snprintf(result.message,sizeof result.message,"%s",message);
    portEXIT_CRITICAL(&result_mux);
    atomic_store(&active_state,state);
}
static void set_count(uint32_t frames)
{
    portENTER_CRITICAL(&result_mux);
    result.captured_frames=frames;
    result.capture_seconds=frames/LF_SCAN_RATE;
    portEXIT_CRITICAL(&result_mux);
}
bool lf_recog_busy(void)
{
    const int s=atomic_load(&active_state);
    return s==LF_RECOG_CAPTURE||s==LF_RECOG_UPLOAD;
}
const char *lf_recog_state_name(lf_recog_state_t state)
{
    switch(state){
    case LF_RECOG_IDLE:return "idle";
    case LF_RECOG_CAPTURE:return "capturing";
    case LF_RECOG_UPLOAD:return "identifying";
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
static bool post_write(esp_http_client_handle_t client,const uint8_t *src,size_t len)
{
    size_t sent=0;
    while(sent<len && !atomic_load(&stop_flag)){
        const size_t chunk=len-sent>4096U?4096U:len-sent;
        const int written=esp_http_client_write(client,(const char *)src+sent,(int)chunk);
        if(written<=0)return false;
        sent+=(size_t)written;
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
    const bool has_match=success&&cJSON_IsString(title)&&cJSON_IsString(artist)&&
        title->valuestring[0]&&artist->valuestring[0];
    const lf_service_error_t mapped=lf_recognition_status(http_code,provider_code,has_match);
    portENTER_CRITICAL(&result_mux);
    result.http_status=http_code;
    if(has_match){
        snprintf(result.title,sizeof result.title,"%s",title->valuestring);
        snprintf(result.artist,sizeof result.artist,"%s",artist->valuestring);
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
    esp_http_client_config_t config={
        .url="https://api.audd.io/",
        .crt_bundle_attach=esp_crt_bundle_attach,
        .timeout_ms=15000,
        .buffer_size=4096,
        .keep_alive_enable=false
    };
    esp_http_client_handle_t client=esp_http_client_init(&config);
    if(!client)return false;
    esp_http_client_set_method(client,HTTP_METHOD_POST);
    esp_http_client_set_header(client,"Content-Type",
          "multipart/form-data; boundary=" LF_RECOG_BOUNDARY);
    bool ok=false;
    if(esp_http_client_open(client,(int)body_len)==ESP_OK){
        if(post_write(client,(const uint8_t*)prefix,(size_t)n)&&
           post_write(client,wav,wav_bytes)&&
           post_write(client,(const uint8_t*)suffix,sizeof suffix-1U)&&
           !atomic_load(&stop_flag)){
            const int64_t header_len=esp_http_client_fetch_headers(client);
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
        frames+=LF_SCAN_TDM_FRAMES;
        set_count(frames);
    }
    (void)esp_codec_dev_close(codec);opened=false;
    if(atomic_load(&stop_flag))goto done;
    if(frames<LF_RECOG_MIN_FRAMES){
        status(LF_RECOG_ERROR,"Not enough recorded audio to identify song");goto done;
    }
    lf_scan_wav_header(wav,frames);
    status(LF_RECOG_UPLOAD,"Identifying song securely with AudD...");
    ok=send_audio(wav,LF_SCAN_WAV_HEADER+(size_t)frames*4U,token);
    if(!ok&&!atomic_load(&stop_flag))status(LF_RECOG_ERROR,
                        "Music recognition connection failed. Try again.");
done:
    if(opened)(void)esp_codec_dev_close(codec);
    if(wav){clear_secret(wav,LF_SCAN_WAV_BYTES);heap_caps_free(wav);}
    clear_secret(token,sizeof token);
    if(atomic_load(&stop_flag))status(LF_RECOG_STOPPED,"Recognition stopped");
    vTaskDelete(NULL);
}
bool lf_recog_start(void)
{
    lf_pcm_snapshot_t pcm={0};
    lf_pcm_snapshot(&pcm);
    if(pcm.state==LF_PCM_RUNNING)return false;
    if(!lf_recog_has_token()||!lf_network_is_connected())return false;
    int state=atomic_load(&active_state);
    while(state!=LF_RECOG_CAPTURE&&state!=LF_RECOG_UPLOAD){
        if(atomic_compare_exchange_weak(&active_state,&state,LF_RECOG_CAPTURE))break;
    }
    if(state==LF_RECOG_CAPTURE||state==LF_RECOG_UPLOAD)return false;
    atomic_store(&stop_flag,false);
    portENTER_CRITICAL(&result_mux);
    memset(&result,0,sizeof result);
    result.state=LF_RECOG_CAPTURE;
    result.attempts=atomic_fetch_add(&attempts,1)+1;
    snprintf(result.message,sizeof result.message,"Recording up to 10 seconds");
    portEXIT_CRITICAL(&result_mux);
    if(xTaskCreate(worker,"lf_song_recog",14336,NULL,tskIDLE_PRIORITY+1,NULL)!=pdPASS){
        status(LF_RECOG_ERROR,"Cannot start recognition task");
        return false;
    }
    return true;
}
void lf_recog_stop(void){atomic_store(&stop_flag,true);}
