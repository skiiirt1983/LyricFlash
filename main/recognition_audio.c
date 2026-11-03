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
/* Same rail and TDM slot definitions as the on-demand PCM test. Keeping
 * this helper separate lets native tests verify exact uploaded-channel data. */
static uint32_t scan_isqrt_u64(uint64_t value)
{
    uint64_t result=0,bit=(uint64_t)1<<62;
    while(bit>value)bit>>=2;
    while(bit) {
        if(value>=result+bit) {
            value-=result+bit;
            result=(result>>1)+bit;
        } else result>>=1;
        bit>>=2;
    }
    return (uint32_t)result;
}
uint32_t lf_scan_audio_rms(uint64_t sum_squares,uint32_t frames)
{
    return frames?scan_isqrt_u64(sum_squares/frames):0;
}
void lf_scan_audio_metrics_add(lf_scan_audio_metrics_t *m,
                               const int16_t *tdm,size_t samples)
{
    if(!m||!tdm||!samples||samples%4U)return;
    for(size_t i=0;i<samples;i+=4) {
        /* TDM slots 0 and 2 alone become the actual stereo upload.
         * Widen to int32 before abs to safely handle INT16_MIN. */
        const int32_t v1=tdm[i],v2=tdm[i+2];
        const uint32_t a1=(uint32_t)(v1<0?-v1:v1);
        const uint32_t a2=(uint32_t)(v2<0?-v2:v2);
        const bool rail1=a1>=LF_SCAN_NEAR_RAIL;
        const bool rail2=a2>=LF_SCAN_NEAR_RAIL;
        m->frames++;
        m->mic1_sum_sq+=(uint64_t)a1*a1;
        m->mic2_sum_sq+=(uint64_t)a2*a2;
        if(a1>m->mic1_peak)m->mic1_peak=a1;
        if(a2>m->mic2_peak)m->mic2_peak=a2;
        if(a1>=LF_SCAN_HALF_SCALE)m->mic1_half_scale++;
        if(a2>=LF_SCAN_HALF_SCALE)m->mic2_half_scale++;
        if(rail1) {
            m->mic1_near_rail++;
            if(!m->mic1_rail_run)m->mic1_rail_bursts++;
            m->mic1_rail_run++;
            if(m->mic1_rail_run>m->mic1_longest_burst)
                m->mic1_longest_burst=m->mic1_rail_run;
        } else {
            m->mic1_rail_run=0;
            m->mic1_below_rail_sq+=(uint64_t)a1*a1;
            m->mic1_below_rail_frames++;
        }
        if(rail2) {
            m->mic2_near_rail++;
            if(!m->mic2_rail_run)m->mic2_rail_bursts++;
            m->mic2_rail_run++;
            if(m->mic2_rail_run>m->mic2_longest_burst)
                m->mic2_longest_burst=m->mic2_rail_run;
        } else {
            m->mic2_rail_run=0;
            m->mic2_below_rail_sq+=(uint64_t)a2*a2;
            m->mic2_below_rail_frames++;
        }
        if(rail1&&rail2)m->both_near_rail++;
    }
}
/* Prevent multipart boundary/header injection or leaking a pasted URL.
 * No credential is ever compiled into firmware or returned by GET. */
bool lf_scan_token_valid(const char *token)
{
    if(!token)return false;
    size_t n=0;
    for(;n<128U;n++){
        const char c=token[n];
        if(!c)break;
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
             (c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'))return false;
    }
    return n>=4U&&n<128U;
}
