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
 * 22050Hz 16-bit mono capture path. The ADC is opened ONLY on explicit
 * Start, receives at most three seconds of short PCM buffers and closes
 * after Stop, timeout, or any read error.
 *
 * The BSP owns the I2C/I2S lifetime; do not delete its codec handle.
 * I2S peripheral clocks can remain configured after esp_codec_dev_close.
 * Audio power-resource teardown requires a separately tested BSP change;
 * no continuous PCM reads or active ES7210 capture are allowed while idle.
 * No buffers are retained, streamed, written to flash, or logged. */
#define LF_PCM_LIMIT_US (3000000LL)
#define LF_PCM_RATE (22050)
static atomic_int state=ATOMIC_VAR_INIT(LF_PCM_IDLE);
static atomic_int error=ATOMIC_VAR_INIT(LF_PCM_ERR_NONE);
static atomic_uint attempts;
static atomic_uint peak_abs;
static atomic_uint mean_abs;
static atomic_uint sample_count;
static atomic_uint elapsed_ms;
static atomic_bool stop_requested;

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
            .sample_rate=LF_PCM_RATE, .channel=1, .bits_per_sample=16
        };
        if(esp_codec_dev_open(codec,&fs)!=ESP_CODEC_DEV_OK)
            fail=LF_PCM_ERR_OPEN;
        else opened=true;
    }
    uint64_t total_abs=0;
    uint32_t samples=0;
    uint32_t peak=0;
    if(opened) {
        while(!atomic_load(&stop_requested) &&
              esp_timer_get_time()-start<LF_PCM_LIMIT_US) {
            int16_t pcm[512]={0};
            if(esp_codec_dev_read(codec,pcm,sizeof(pcm))!=ESP_CODEC_DEV_OK) {
                fail=LF_PCM_ERR_READ;
                break;
            }
            for(size_t i=0;i<sizeof(pcm)/sizeof(pcm[0]);i++) {
                int32_t val=pcm[i];
                uint32_t magnitude=(uint32_t)(val<0?-val:val);
                total_abs+=magnitude;
                if(magnitude>peak)peak=magnitude;
            }
            samples+=(uint32_t)(sizeof(pcm)/sizeof(pcm[0]));
            atomic_store(&peak_abs,peak);
            atomic_store(&sample_count,samples);
            atomic_store(&mean_abs,(uint32_t)(total_abs/samples));
        }
        /* Audio ADC must stop even after read failure or user Stop. */
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
