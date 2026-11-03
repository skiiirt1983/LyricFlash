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
