#include "recognition_audio.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
static uint16_t le16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
int main(void)
{
    assert(LF_SCAN_RATE==24000U);
    assert(LF_SCAN_MAX_FRAMES==240000U);
    assert(LF_SCAN_WAV_BYTES==960044U);
    uint8_t header[44];
    memset(header,0,sizeof header);
    lf_scan_wav_header(header,240000);
    assert(memcmp(header,"RIFF",4)==0 && le32(header+4)==960036);
    assert(memcmp(header+8,"WAVEfmt ",8)==0 && le32(header+16)==16);
    assert(le16(header+20)==1 && le16(header+22)==2);
    assert(le32(header+24)==24000 && le32(header+28)==96000);
    assert(le16(header+32)==4 && le16(header+34)==16);
    assert(memcmp(header+36,"data",4)==0 && le32(header+40)==960000);
    lf_scan_wav_header(header,5);
    assert(le32(header+40)==20 && le32(header+4)==56);
    int16_t tdm[8]={-32768,111,32767,222,300,-500,-1000,777};
    uint8_t bytes[8]={0};
    assert(lf_scan_tdm_to_stereo(bytes,8,tdm,8)==8);
    assert(le16(bytes)==32768 && le16(bytes+2)==32767);
    assert(le16(bytes+4)==300 && le16(bytes+6)==(uint16_t)-1000);
    assert(lf_scan_tdm_to_stereo(bytes,7,tdm,8)==0);
    assert(lf_scan_tdm_to_stereo(bytes,8,tdm,7)==0);
    assert(lf_scan_tdm_to_stereo(bytes,8,tdm,0)==0);
    assert(lf_scan_tdm_to_stereo(NULL,8,tdm,8)==0);
    /* Measure the identical MIC1/MIC2 TDM slots that form the AudD WAV.
     * Two rail samples in the first frame must count as a single burst
     * per microphone; other slots must have no influence. */
    lf_scan_audio_metrics_t levels={0};
    lf_scan_audio_metrics_add(&levels,tdm,8);
    assert(levels.frames==2);
    assert(levels.mic1_peak==32768 && levels.mic2_peak==32767);
    assert(levels.mic1_near_rail==1 && levels.mic2_near_rail==1);
    assert(levels.both_near_rail==1);
    assert(levels.mic1_half_scale==1 && levels.mic2_half_scale==1);
    assert(levels.mic1_rail_bursts==1 && levels.mic2_rail_bursts==1);
    assert(levels.mic1_longest_burst==1 && levels.mic2_longest_burst==1);
    assert(levels.mic1_below_rail_frames==1 && levels.mic2_below_rail_frames==1);
    assert(lf_scan_audio_rms(0,0)==0);
    assert(lf_scan_audio_rms(10000,4)==50);
    assert(lf_scan_audio_rms(levels.mic1_below_rail_sq,
                            levels.mic1_below_rail_frames)==300);
    assert(lf_scan_audio_rms(levels.mic2_below_rail_sq,
                            levels.mic2_below_rail_frames)==1000);
    const int16_t rail_burst[12]={32767,300,32767,9,
        -32768,101,-32768,-2,0,200,0,110};
    lf_scan_audio_metrics_add(&levels,rail_burst,12);
    assert(levels.frames==5 && levels.mic1_near_rail==3);
    assert(levels.mic1_rail_bursts==2 && levels.mic1_longest_burst==2);
    assert(levels.mic2_rail_bursts==2 && levels.mic2_longest_burst==2);
    lf_scan_audio_metrics_add(&levels,NULL,12);
    lf_scan_audio_metrics_add(&levels,rail_burst,11);
    assert(levels.frames==5);
    assert(lf_scan_token_valid("test"));
    assert(lf_scan_token_valid("abcd-0123_XYZ.token"));
    assert(!lf_scan_token_valid(""));
    assert(!lf_scan_token_valid("abc"));
    assert(!lf_scan_token_valid("abcd\nX-Injected: yes"));
    assert(!lf_scan_token_valid("abcd \t efgh"));
    assert(!lf_scan_token_valid("http://example.com/key"));
    char oversized[140];memset(oversized,'x',sizeof oversized);oversized[139]=0;
    assert(!lf_scan_token_valid(oversized));
    puts("PASS: 24kHz 16-bit stereo WAV, MIC1/MIC2 TDM mapping, buffer bounds, token injection checks");
    return 0;
}
