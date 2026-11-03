#include "recognition_audio.h"
#include <string.h>

/* Little-endian RIFF and PCM fields, independent of host endianness. */
static void u16le(uint8_t *p, uint16_t n) { p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8); }
static void u32le(uint8_t *p, uint32_t n) {
    for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8U*i));
}
void lf_scan_wav_header(uint8_t header[44], uint32_t frames)
{
    if(!header)return;
    if(frames>LF_SCAN_MAX_FRAMES)frames=LF_SCAN_MAX_FRAMES;
    const uint32_t data=frames*LF_SCAN_CHANNELS*2U;
    memcpy(header,"RIFF",4);
    u32le(header+4,36U+data);
    memcpy(header+8,"WAVEfmt ",8);
    u32le(header+16,16);     /* fmt subchunk size */
    u16le(header+20,1);      /* uncompressed PCM */
    u16le(header+22,2);      /* independent front microphones */
    u32le(header+24,LF_SCAN_RATE);
    u32le(header+28,LF_SCAN_RATE*4U);
    u16le(header+32,4);      /* block alignment */
    u16le(header+34,16);     /* bits per sample */
    memcpy(header+36,"data",4);
    u32le(header+40,data);
}
size_t lf_scan_tdm_to_stereo(uint8_t *dest, size_t cap,
                             const int16_t *tdm, size_t samples)
{
    if(!dest||!tdm||!samples||samples%4U)return 0;
    const size_t out=samples;
    if(cap<out)return 0;     /* four slots -> two channels * two bytes */
    for(size_t i=0,j=0;i<samples;i+=4,j+=4){
        const uint16_t ch1=(uint16_t)tdm[i];
        const uint16_t ch2=(uint16_t)tdm[i+2];
        u16le(dest+j,ch1);
        u16le(dest+j+2,ch2);
    }
    return out;
}
/* Prevent multipart boundary/header injection or leaking a pasted URL.
 * No credential is ever compiled into firmware or returned by GET. */
bool lf_scan_token_valid(const char *token)
{
    if(!token)return false;
    size_t n=0;
    while(token[n]&&n<=128U){
        const char c=token[n++];
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
             (c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'))return false;
    }
    return n>=4U&&n<=127U;
}
