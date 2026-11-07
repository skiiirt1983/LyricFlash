#include "mic_pcm.h"
#include "bsp/esp-bsp.h"
#include "bsp/esp32_s3_touch_amoled_1_75.h"
#include "esp_codec_dev.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Stage 2A: smallest useful live ADC/PCM test using the BSP's existing
 * Waveshare 24kHz 4-slot, 16-bit TDM capture path. ADC opens ONLY on explicit
 * Start, receives at most three seconds of short PCM buffers and closes
 * after Stop, timeout, or any read error.
 *
 * The BSP owns the I2C/I2S lifetime; do not delete its codec handle.
 * I2S peripheral clocks can remain configured after esp_codec_dev_close.
 * Audio power-resource teardown requires a separately tested BSP change;
 * no continuous PCM reads or active ES7210 capture are allowed while idle.
 * No buffers are retained, streamed, written to flash, or logged. */
#define LF_PCM_LIMIT_US (3000000LL)
#define LF_PCM_RATE (24000)
#define LF_PCM_TDM_SLOTS (4)
#define LF_PCM_FRONT_GAIN_DB (27.0f)
#define LF_PCM_FRONT_PHYSICAL_MASK (0x03U)
#define LF_PCM_TDM_SLOT_MASK (0x0FU)
/* Samples within 8 integer units of the signed 16-bit ADC rails.
 * This flags possible clipping or sporadic transport artifacts, not proof. */
#define LF_PCM_NEAR_RAIL (32760U)
#define LF_PCM_HALF_SCALE (16384U)
static atomic_int state=ATOMIC_VAR_INIT(LF_PCM_IDLE);
static atomic_int error=ATOMIC_VAR_INIT(LF_PCM_ERR_NONE);
static atomic_uint attempts;
static atomic_uint peak_abs;
static atomic_uint mean_abs;
static atomic_uint sample_count;
static atomic_uint elapsed_ms;
static atomic_uint mic1_mean_abs, mic1_peak_abs;
static atomic_uint mic2_mean_abs, mic2_peak_abs;
static atomic_uint mic1_rms, mic2_rms;
static atomic_uint mic1_near_rail, mic2_near_rail;
static atomic_uint mic1_rail_bursts, mic2_rail_bursts;
static atomic_uint mic1_longest_rail_burst, mic2_longest_rail_burst;
static atomic_uint mic1_rms_below_rail, mic2_rms_below_rail;
static atomic_uint mic1_half_scale, mic2_half_scale;
static atomic_uint both_near_rail_frames;
static atomic_bool stop_requested;

/* Integer sqrt of a 64-bit sum-of-squares quotient, bounded to <= 32768.
 * Avoid libm and floating point work in the hot 24 kHz capture path. */
static uint32_t pcm_isqrt_u64(uint64_t value)
{
    uint64_t result=0;
    uint64_t bit=(uint64_t)1<<62;
    while(bit>value)bit>>=2;
    while(bit!=0) {
        if(value>=result+bit) {
            value-=result+bit;
            result=(result>>1)+bit;
        } else {
            result>>=1;
        }
        bit>>=2;
    }
    return (uint32_t)result;
}

const char *lf_pcm_state_name(lf_pcm_state_t s)
{
    switch(s) {
    case LF_PCM_IDLE:return "idle";
    case LF_PCM_RUNNING:return "capturing";
    case LF_PCM_COMPLETE:return "complete";
    case LF_PCM_ERROR:return "error";
    default:return "unknown";
    }
}
const char *lf_pcm_error_name(lf_pcm_error_t e)
{
    switch(e) {
    case LF_PCM_ERR_NONE:return "none";
    case LF_PCM_ERR_TASK:return "task_start";
    case LF_PCM_ERR_CODEC:return "codec_init";
    case LF_PCM_ERR_OPEN:return "codec_open";
    case LF_PCM_ERR_READ:return "audio_read";
    case LF_PCM_ERR_EMPTY:return "no_samples";
    case LF_PCM_ERR_GAIN:return "mic_gain";
    default:return "unknown";
    }
}
void lf_pcm_snapshot(lf_pcm_snapshot_t *out)
{
    if(!out)return;
    out->state=(lf_pcm_state_t)atomic_load(&state);
    out->error=(lf_pcm_error_t)atomic_load(&error);
    out->attempts=atomic_load(&attempts);
    out->peak_abs=atomic_load(&peak_abs);
    out->mean_abs=atomic_load(&mean_abs);
    out->sample_count=atomic_load(&sample_count);
    out->elapsed_ms=atomic_load(&elapsed_ms);
    out->mic1_mean_abs=atomic_load(&mic1_mean_abs);
    out->mic1_peak_abs=atomic_load(&mic1_peak_abs);
    out->mic2_mean_abs=atomic_load(&mic2_mean_abs);
    out->mic2_peak_abs=atomic_load(&mic2_peak_abs);
    out->mic1_rms=atomic_load(&mic1_rms);
    out->mic2_rms=atomic_load(&mic2_rms);
    out->mic1_near_rail=atomic_load(&mic1_near_rail);
    out->mic2_near_rail=atomic_load(&mic2_near_rail);
    out->mic1_rail_bursts=atomic_load(&mic1_rail_bursts);
    out->mic2_rail_bursts=atomic_load(&mic2_rail_bursts);
    out->mic1_longest_rail_burst=atomic_load(&mic1_longest_rail_burst);
    out->mic2_longest_rail_burst=atomic_load(&mic2_longest_rail_burst);
    out->mic1_rms_below_rail=atomic_load(&mic1_rms_below_rail);
    out->mic2_rms_below_rail=atomic_load(&mic2_rms_below_rail);
    out->mic1_half_scale=atomic_load(&mic1_half_scale);
    out->mic2_half_scale=atomic_load(&mic2_half_scale);
    out->both_near_rail_frames=atomic_load(&both_near_rail_frames);
    out->stop_requested=atomic_load(&stop_requested);
}
static void capture_worker(void *arg)
{
    (void)arg;
    lf_pcm_error_t fail=LF_PCM_ERR_NONE;
    esp_codec_dev_handle_t codec=bsp_audio_codec_microphone_init();
    int64_t start=esp_timer_get_time();
    bool opened=false;
    if(!codec) fail=LF_PCM_ERR_CODEC;
    if(fail==LF_PCM_ERR_NONE && !atomic_load(&stop_requested)) {
        esp_codec_dev_sample_info_t fs={
            .sample_rate=LF_PCM_RATE, .channel=LF_PCM_TDM_SLOTS,
            .channel_mask=LF_PCM_TDM_SLOT_MASK, .bits_per_sample=16
        };
        if(esp_codec_dev_open(codec,&fs)!=ESP_CODEC_DEV_OK) {
            fail=LF_PCM_ERR_OPEN;
        } else {
            opened=true;
            if(esp_codec_dev_set_in_channel_gain(
                  codec, LF_PCM_FRONT_PHYSICAL_MASK, LF_PCM_FRONT_GAIN_DB
                )!=ESP_CODEC_DEV_OK)
                fail=LF_PCM_ERR_GAIN;
        }
    }
    uint64_t total_abs=0, mic1_total=0, mic2_total=0;
    uint64_t mic1_sum_sq=0, mic2_sum_sq=0;
    uint32_t mic1_clip=0, mic2_clip=0;
    uint32_t mic1_bursts=0, mic2_bursts=0, mic1_run=0, mic2_run=0;
    uint32_t mic1_longest=0, mic2_longest=0, both_rail=0;
    uint32_t mic1_half=0, mic2_half=0, mic1_below_count=0, mic2_below_count=0;
    uint64_t mic1_below_sq=0, mic2_below_sq=0;
    uint32_t samples=0, frames=0, peak=0, mic1_peak=0, mic2_peak=0;
    if(opened) {
        if(fail==LF_PCM_ERR_NONE) {
            while(!atomic_load(&stop_requested) &&
                  esp_timer_get_time()-start<LF_PCM_LIMIT_US) {
                /* Interleaved [MIC1, MIC3(ES8311 playback reference),
                 * MIC2, MIC4(unconnected)]. Never measure MIC3/MIC4. */
                int16_t pcm[512]={0};
                if(esp_codec_dev_read(codec,pcm,sizeof(pcm))!=ESP_CODEC_DEV_OK) {
                    fail=LF_PCM_ERR_READ;
                    break;
                }
                for(size_t i=0;i<sizeof(pcm)/sizeof(pcm[0]);i+=LF_PCM_TDM_SLOTS) {
                    int32_t v1=pcm[i], v2=pcm[i+2];
                    uint32_t a1=(uint32_t)(v1<0?-v1:v1);
                    uint32_t a2=(uint32_t)(v2<0?-v2:v2);
                    mic1_total+=a1;mic2_total+=a2;
                    /* Signed 16-bit MIN (-32768) is handled by int32_t. */
                    mic1_sum_sq+=(uint64_t)a1*a1;
                    mic2_sum_sq+=(uint64_t)a2*a2;
                    /* Preserve frame adjacency across read-buffer boundaries.
                     * Bursts distinguish isolated one-sample artifacts from
                     * contiguous clipping; no raw audio retained. */
                    const bool rail1=a1>=LF_PCM_NEAR_RAIL;
                    const bool rail2=a2>=LF_PCM_NEAR_RAIL;
                    if(a1>=LF_PCM_HALF_SCALE)mic1_half++;
                    if(a2>=LF_PCM_HALF_SCALE)mic2_half++;
                    if(rail1) {
                        mic1_clip++;
                        if(mic1_run==0)mic1_bursts++;
                        mic1_run++;
                        if(mic1_run>mic1_longest)mic1_longest=mic1_run;
                    } else {
                        mic1_run=0;
                        mic1_below_sq+=(uint64_t)a1*a1;
                        mic1_below_count++;
                    }
                    if(rail2) {
                        mic2_clip++;
                        if(mic2_run==0)mic2_bursts++;
                        mic2_run++;
                        if(mic2_run>mic2_longest)mic2_longest=mic2_run;
                    } else {
                        mic2_run=0;
                        mic2_below_sq+=(uint64_t)a2*a2;
                        mic2_below_count++;
                    }
                    if(rail1&&rail2)both_rail++;
                    total_abs+=a1+a2;
                    if(a1>mic1_peak)mic1_peak=a1;
                    if(a2>mic2_peak)mic2_peak=a2;
                    if(a1>peak)peak=a1;
                    if(a2>peak)peak=a2;
                }
                frames+=(uint32_t)(sizeof(pcm)/sizeof(pcm[0])/LF_PCM_TDM_SLOTS);
                samples=frames*2U;
                atomic_store(&sample_count,samples);
                atomic_store(&peak_abs,peak);
                atomic_store(&mean_abs,(uint32_t)(total_abs/samples));
                atomic_store(&mic1_peak_abs,mic1_peak);
                atomic_store(&mic2_peak_abs,mic2_peak);
                atomic_store(&mic1_mean_abs,(uint32_t)(mic1_total/frames));
                atomic_store(&mic2_mean_abs,(uint32_t)(mic2_total/frames));
                /* Integer RMS is floor(sqrt(mean(sample^2))), per channel. */
                atomic_store(&mic1_rms,pcm_isqrt_u64(mic1_sum_sq/frames));
                atomic_store(&mic2_rms,pcm_isqrt_u64(mic2_sum_sq/frames));
                atomic_store(&mic1_near_rail,mic1_clip);
                atomic_store(&mic2_near_rail,mic2_clip);
                atomic_store(&mic1_rail_bursts,mic1_bursts);
                atomic_store(&mic2_rail_bursts,mic2_bursts);
                atomic_store(&mic1_longest_rail_burst,mic1_longest);
                atomic_store(&mic2_longest_rail_burst,mic2_longest);
                atomic_store(&mic1_rms_below_rail,mic1_below_count?
                    pcm_isqrt_u64(mic1_below_sq/mic1_below_count):0);
                atomic_store(&mic2_rms_below_rail,mic2_below_count?
                    pcm_isqrt_u64(mic2_below_sq/mic2_below_count):0);
                atomic_store(&mic1_half_scale,mic1_half);
                atomic_store(&mic2_half_scale,mic2_half);
                atomic_store(&both_near_rail_frames,both_rail);
            }
        }
        /* Close ES7210 ADC even after gain/read failure or early Stop. */
        (void)esp_codec_dev_close(codec);
    }
    if(fail==LF_PCM_ERR_NONE && !samples && !atomic_load(&stop_requested))
        fail=LF_PCM_ERR_EMPTY;
    atomic_store(&elapsed_ms,(uint32_t)((esp_timer_get_time()-start)/1000LL));
    atomic_store(&error,fail);
    atomic_store(&state,fail==LF_PCM_ERR_NONE?LF_PCM_COMPLETE:LF_PCM_ERROR);
    vTaskDelete(NULL);
}
bool lf_pcm_start(void)
{
    int expected=atomic_load(&state);
    while(expected!=LF_PCM_RUNNING) {
        if(atomic_compare_exchange_weak(&state,&expected,LF_PCM_RUNNING))break;
    }
    if(expected==LF_PCM_RUNNING)return false;
    atomic_store(&stop_requested,false);
    atomic_store(&error,LF_PCM_ERR_NONE);
    atomic_store(&peak_abs,0);
    atomic_store(&mean_abs,0);
    atomic_store(&sample_count,0);
    atomic_store(&elapsed_ms,0);
    atomic_store(&mic1_mean_abs,0);atomic_store(&mic1_peak_abs,0);
    atomic_store(&mic2_mean_abs,0);atomic_store(&mic2_peak_abs,0);
    atomic_store(&mic1_rms,0);atomic_store(&mic2_rms,0);
    atomic_store(&mic1_near_rail,0);atomic_store(&mic2_near_rail,0);
    atomic_store(&mic1_rail_bursts,0);atomic_store(&mic2_rail_bursts,0);
    atomic_store(&mic1_longest_rail_burst,0);atomic_store(&mic2_longest_rail_burst,0);
    atomic_store(&mic1_rms_below_rail,0);atomic_store(&mic2_rms_below_rail,0);
    atomic_store(&mic1_half_scale,0);atomic_store(&mic2_half_scale,0);
    atomic_store(&both_near_rail_frames,0);
    atomic_fetch_add(&attempts,1);
    if(xTaskCreate(capture_worker,"lf_mic_pcm",6144,NULL,
                   tskIDLE_PRIORITY+1,NULL)!=pdPASS) {
        atomic_store(&error,LF_PCM_ERR_TASK);
        atomic_store(&state,LF_PCM_ERROR);
        return false;
    }
    return true;
}
void lf_pcm_stop(void) { atomic_store(&stop_requested,true); }
