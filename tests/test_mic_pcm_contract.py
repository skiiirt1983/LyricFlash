#!/usr/bin/env python3
"""Stage-2a native host harness: test manual-only PCM state transitions."""
from pathlib import Path
from textwrap import dedent
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/"main/mic_pcm.c").read_text()
net=(root/"main/network.c").read_text()
page=(root/"main/web_device_html.h").read_text()
cmake=(root/"main/CMakeLists.txt").read_text()
assert "esp_codec_dev_read(codec,pcm,sizeof(pcm))" in source
assert "LF_PCM_LIMIT_US (3000000LL)" in source
assert "LF_PCM_NEAR_RAIL (32760U)" in source
assert "pcm_isqrt_u64" in source
assert "mic1_near_rail" in net and "mic2_near_rail" in net
assert "mic1_rms" in page and "mic2_rms" in page
assert "mic1_rail_bursts" in net and "mic2_longest_rail_burst" in net
assert "mic1_rms_below_rail" in page and "both_near_rail_frames" in page
assert "LF_PCM_HALF_SCALE (16384U)" in source
assert "esp_codec_dev_close(codec)" in source
assert "atomic_load(&stop_requested)" in source
assert "vTaskDelay" not in source
assert "fwrite" not in source and "fopen" not in source
assert "esp_http_client" not in source and "httpd_resp_send" not in source
assert "mic_pcm.c" in cmake
assert "X-LyricFlash-Manual-Test" in net and "X-LyricFlash-Manual-Test" in page
for p in ('/api/hardware/mic-test','/api/hardware/mic-test/start','/api/hardware/mic-test/stop'):
    assert p in net, p
    assert p in page, p

with tempfile.TemporaryDirectory(prefix="lf_pcm_test_") as tmp:
    p=Path(tmp)
    for d in ("bsp","freertos"):
        (p/d).mkdir()
    (p/"esp_codec_dev.h").write_text(dedent("""
        #pragma once
        #include <stdint.h>
        typedef void *esp_codec_dev_handle_t;
        typedef struct {int sample_rate, channel, bits_per_sample; unsigned channel_mask;} esp_codec_dev_sample_info_t;
        #define ESP_CODEC_DEV_OK 0
        int esp_codec_dev_open(esp_codec_dev_handle_t,esp_codec_dev_sample_info_t *);
        int esp_codec_dev_read(esp_codec_dev_handle_t,void *,int);
        int esp_codec_dev_close(esp_codec_dev_handle_t);
        int esp_codec_dev_set_in_channel_gain(esp_codec_dev_handle_t,unsigned,float);
    """))
    (p/"esp_timer.h").write_text(dedent("""
        #pragma once
        #include <stdint.h>
        int64_t esp_timer_get_time(void);
    """))
    (p/"bsp/esp-bsp.h").write_text("#pragma once\n")
    (p/"bsp/esp32_s3_touch_amoled_1_75.h").write_text(dedent("""
        #pragma once
        #include "esp_codec_dev.h"
        esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void);
    """))
    (p/"freertos/FreeRTOS.h").write_text(dedent("""
        #pragma once
        typedef int BaseType_t;
        #define pdPASS 1
        #define tskIDLE_PRIORITY 0
    """))
    (p/"freertos/task.h").write_text(dedent("""
        #pragma once
        #include "freertos/FreeRTOS.h"
        BaseType_t xTaskCreate(void(*fn)(void*),const char *name,unsigned stack,
                                void *arg,int priority,void *out);
        void vTaskDelete(void *);
    """))
    (p/"test.c").write_text(dedent("""
        #include "mic_pcm.h"
        #include "esp_codec_dev.h"
        #include "freertos/task.h"
        #include <assert.h>
        #include <stdint.h>
        static int codec_exists=1,open_ok=1,read_ok=1,spawn_ok=1,request_stop=0,gain_ok=1,rail_mode=0,silent_mode=0;
        static int inits=0,opens=0,reads=0,closes=0,gains=0;
        static int64_t time_us=0;
        int64_t esp_timer_get_time(void){time_us+=300000;return time_us;}
        esp_codec_dev_handle_t bsp_audio_codec_microphone_init(void)
        {inits++;return codec_exists?(void*)0x1:0;}
        int esp_codec_dev_open(esp_codec_dev_handle_t c,esp_codec_dev_sample_info_t *f)
        {assert(c);assert(f->sample_rate==24000&&f->channel==4&&f->channel_mask==0xF&&f->bits_per_sample==16);
         opens++;return open_ok?ESP_CODEC_DEV_OK:-1;}
        int esp_codec_dev_set_in_channel_gain(esp_codec_dev_handle_t c,unsigned mask,float gain)
        {assert(c&&mask==0x03&&gain==24.0f);gains++;return gain_ok?ESP_CODEC_DEV_OK:-1;}
        int esp_codec_dev_read(esp_codec_dev_handle_t c,void *ptr,int bytes)
        {assert(c&&bytes==1024);reads++;if(!read_ok)return -1;
         int16_t *buf=ptr;for(int i=0;i<512;i+=4){buf[i]=silent_mode?0:1000;buf[i+1]=32767;buf[i+2]=silent_mode?0:-2000;buf[i+3]=-32768;}
         if(rail_mode==1){buf[0]=-32768;buf[2]=32767;}
         if(rail_mode==2){for(int j=0;j<3;j++)buf[j*4]=-32768;
                           for(int j=0;j<2;j++)buf[j*4+2]=32767;}
         if(rail_mode==3){for(int j=0;j<2;j++){buf[j*4]=-32768;
                            buf[(126+j)*4]=-32768;}}
         if(request_stop)lf_pcm_stop();
         return ESP_CODEC_DEV_OK;}
        int esp_codec_dev_close(esp_codec_dev_handle_t c)
        {assert(c);closes++;return ESP_CODEC_DEV_OK;}
        BaseType_t xTaskCreate(void(*fn)(void*),const char *name,unsigned stack,
                              void *arg,int priority,void *out)
        {(void)name;(void)stack;(void)priority;(void)out;
         if(!spawn_ok){return 0;}
         fn(arg);
         return pdPASS;}
        void vTaskDelete(void *arg){(void)arg;}
        int main(void){
            lf_pcm_snapshot_t s={0};lf_pcm_snapshot(&s);
            assert(s.state==LF_PCM_IDLE && inits==0 && reads==0);
            assert(lf_pcm_start());
            lf_pcm_snapshot(&s);
            assert(s.state==LF_PCM_COMPLETE && s.error==LF_PCM_ERR_NONE);
            assert(s.sample_count>0 && s.peak_abs==2000 && s.mean_abs==1500);
            assert(s.mic1_mean_abs==1000 && s.mic1_peak_abs==1000);
            assert(s.mic2_mean_abs==2000 && s.mic2_peak_abs==2000);
            assert(s.mic1_rms==1000 && s.mic2_rms==2000);
            assert(s.mic1_near_rail==0 && s.mic2_near_rail==0);
            assert(s.mic1_rms_below_rail==1000 && s.mic2_rms_below_rail==2000);
            assert(s.mic1_rail_bursts==0 && s.mic2_longest_rail_burst==0);
            assert(s.mic1_half_scale==0 && s.mic2_half_scale==0);
            assert(s.both_near_rail_frames==0);
            assert(gains==1);
            assert(opens==1 && closes==1);
            request_stop=1;time_us=0;
            assert(lf_pcm_start());lf_pcm_snapshot(&s);
            assert(s.state==LF_PCM_COMPLETE && s.stop_requested);
            assert(closes==2);
            request_stop=0;read_ok=0;time_us=0;
            assert(lf_pcm_start());lf_pcm_snapshot(&s);
            assert(s.state==LF_PCM_ERROR && s.error==LF_PCM_ERR_READ && closes==3);
            read_ok=1;open_ok=0;time_us=0;
            assert(lf_pcm_start());lf_pcm_snapshot(&s);
            assert(s.state==LF_PCM_ERROR && s.error==LF_PCM_ERR_OPEN && closes==3);
            open_ok=1;gain_ok=0;time_us=0;
            assert(lf_pcm_start());lf_pcm_snapshot(&s);
            assert(s.state==LF_PCM_ERROR && s.error==LF_PCM_ERR_GAIN && closes==4);
            gain_ok=1;codec_exists=0;time_us=0;
            assert(lf_pcm_start());lf_pcm_snapshot(&s);
            assert(s.state==LF_PCM_ERROR && s.error==LF_PCM_ERR_CODEC);
            codec_exists=1;spawn_ok=0;
            assert(!lf_pcm_start());lf_pcm_snapshot(&s);
            assert(s.state==LF_PCM_ERROR && s.error==LF_PCM_ERR_TASK);
            /* One near-rail sample per 128 TDM frames; unrelated slots 1
               and 3 at rails MUST NOT count as front microphone clipping. */
            spawn_ok=1;rail_mode=1;time_us=0;
            const int prev_reads=reads;
            assert(lf_pcm_start());lf_pcm_snapshot(&s);
            assert(s.state==LF_PCM_COMPLETE);
            assert(s.mic1_near_rail==(unsigned)(reads-prev_reads));
            assert(s.mic2_near_rail==(unsigned)(reads-prev_reads));
            assert(s.mic1_rail_bursts==(unsigned)(reads-prev_reads));
            assert(s.mic2_rail_bursts==(unsigned)(reads-prev_reads));
            assert(s.mic1_longest_rail_burst==1 && s.mic2_longest_rail_burst==1);
            assert(s.both_near_rail_frames==(unsigned)(reads-prev_reads));
            assert(s.mic1_half_scale==s.mic1_near_rail && s.mic2_half_scale==s.mic2_near_rail);
            assert(s.mic1_rms_below_rail==1000 && s.mic2_rms_below_rail==2000);
            assert(s.mic1_rms>1000 && s.mic2_rms>2000);
            assert(s.mic1_rms<32768 && s.mic2_rms<32768);
            /* Consecutive bursts: 3 frames MIC1, 2 frames MIC2.
               Each buffer has a new independent burst. */
            rail_mode=2;time_us=0;int prev_reads_burst=reads;
            assert(lf_pcm_start());lf_pcm_snapshot(&s);
            assert(s.mic1_near_rail==3U*(unsigned)(reads-prev_reads_burst));
            assert(s.mic2_near_rail==2U*(unsigned)(reads-prev_reads_burst));
            assert(s.mic1_rail_bursts==(unsigned)(reads-prev_reads_burst));
            assert(s.mic2_rail_bursts==(unsigned)(reads-prev_reads_burst));
            assert(s.mic1_longest_rail_burst==3 && s.mic2_longest_rail_burst==2);
            assert(s.both_near_rail_frames==2U*(unsigned)(reads-prev_reads_burst));
            assert(s.mic1_rms_below_rail==1000 && s.mic2_rms_below_rail==2000);
            /* Run spanning an I2S read boundary must not split in two.
               There are 2 rail frames at both ends of each read. */
            rail_mode=3;time_us=0;int prev_reads_cross=reads;
            assert(lf_pcm_start());lf_pcm_snapshot(&s);
            assert(s.mic1_near_rail==4U*(unsigned)(reads-prev_reads_cross));
            assert(s.mic2_near_rail==0);
            assert(s.mic1_longest_rail_burst==4);
            assert(s.mic1_rail_bursts==(unsigned)(reads-prev_reads_cross)+1U);
            assert(s.mic2_rail_bursts==0 && s.both_near_rail_frames==0);
            assert(s.mic1_rms_below_rail==1000 && s.mic2_rms_below_rail==2000);
            rail_mode=0;time_us=0;
            assert(lf_pcm_start());lf_pcm_snapshot(&s);
            assert(s.state==LF_PCM_COMPLETE);
            assert(s.mic1_near_rail==0 && s.mic2_near_rail==0);
            assert(s.mic1_rms==1000 && s.mic2_rms==2000);
            assert(s.mic1_rms_below_rail==1000 && s.mic2_rms_below_rail==2000);
            assert(s.mic1_rail_bursts==0 && s.mic2_longest_rail_burst==0);
            assert(s.both_near_rail_frames==0);
            silent_mode=1;time_us=0;
            assert(lf_pcm_start());lf_pcm_snapshot(&s);
            assert(s.state==LF_PCM_COMPLETE && s.sample_count>0);
            assert(s.mic1_rms==0 && s.mic2_rms==0);
            assert(s.mic1_near_rail==0 && s.mic2_near_rail==0);
            assert(s.mic1_rms_below_rail==0 && s.mic2_rms_below_rail==0);
            assert(s.mic1_rail_bursts==0 && s.mic2_rail_bursts==0);
            assert(s.both_near_rail_frames==0);
            return 0;
        }
    """))
    executable=p/"mic_test"
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",
                    "-I",str(p),"-I",str(root/"main"),str(root/"main/mic_pcm.c"),
                    str(p/"test.c"),"-o",str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
print("PASS: two-mic rail-burst duration, cross-buffer continuity, simultaneous frames, sub-rail RMS, half-scale counts, silence, resets, Stop/errors")
