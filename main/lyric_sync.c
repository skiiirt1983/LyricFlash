#include "lyric_sync.h"
#include <ctype.h>
#include <limits.h>
#include <stddef.h>
/* AudD HH:MM:SS or MM:SS, optionally with fractional seconds. */
bool lf_sync_parse_timecode_ms(const char *s,uint32_t *out){
 if(!s||!out)return false;
 uint64_t fields[3]={0};unsigned n=0;
 while(1){
  if(n==3||!isdigit((unsigned char)*s))return false;
  unsigned digits=0;
  while(isdigit((unsigned char)*s)){if(++digits>3)return false;fields[n]=fields[n]*10+(*s++-'0');}
  n++;
  if(*s!=':')break;
  s++;
 }
 if(n<2||fields[n-1]>=60||(n==3&&fields[1]>=60))return false;
 uint64_t ms=(n==3?fields[0]*3600+fields[1]*60+fields[2]:fields[0]*60+fields[1])*1000;
 if(*s=='.'){
  s++;unsigned digits=0;uint32_t frac=0;
  while(isdigit((unsigned char)*s)){if(++digits>3)return false;frac=frac*10+(*s++-'0');}
  if(!digits)return false;
  while(digits++<3)frac*=10;
  ms+=frac;
 }
 if(*s||ms>UINT32_MAX)return false;
 *out=(uint32_t)ms;return true;
}
uint32_t lf_sync_estimate_position_ms(uint32_t audd,uint64_t start,uint32_t frames,uint32_t rate,uint64_t now){
 if(!rate||now<start)return audd;
 uint64_t capture_ms=(uint64_t)frames*1000/rate;
 uint64_t midpoint=start+capture_ms/2;
 uint64_t delta=now>midpoint?now-midpoint:0;
 uint64_t position=(uint64_t)audd+delta;
 return position>UINT32_MAX?UINT32_MAX:(uint32_t)position;
}

/* Adjust the rendered timeline only, never an on-device persisted offset. */
uint32_t lf_sync_visible_position_ms(uint32_t estimated_ms){
 return estimated_ms>LF_SYNC_DISPLAY_DELAY_MS?
        estimated_ms-LF_SYNC_DISPLAY_DELAY_MS:0U;
}
/* Stay still through long pauses, moving only in the final 1000ms. */
uint16_t lf_sync_slide_fraction(uint32_t position_ms,uint32_t start_ms,uint32_t next_ms){
 if(next_ms<=start_ms)return 1024U;
 if(position_ms>=next_ms)return 1024U;
 const uint32_t gap=next_ms-start_ms;
 const uint32_t window=gap<1000U?gap:1000U;
 const uint32_t motion_start=next_ms-window;
 if(position_ms<=motion_start)return 0U;
 return (uint16_t)(((uint64_t)(position_ms-motion_start)*1024U)/window);
}
/* Fit wrapped height and add capped visual whitespace on long gaps. */
uint16_t lf_sync_row_pitch_px(uint16_t height_a,uint16_t height_b,uint32_t gap_ms){
 uint32_t pitch=((uint32_t)height_a+height_b+1U)/2U+18U;
 if(pitch<92U)pitch=92U;
 if(gap_ms>3500U){
  uint32_t extra=(gap_ms-3500U)/500U*5U;
  if(extra>35U)extra=35U;
  pitch+=extra;
 }
 if(pitch>320U)pitch=320U;
 return (uint16_t)pitch;
}

/* A song's speed can change mid-line without jumping. Store the current
   position as the new anchor every time the user moves the tempo slider. */
uint32_t lf_sync_rate_position(uint32_t base_ms,uint32_t anchor_base_ms,
                               uint32_t anchor_song_ms,uint16_t rate_percent){
 if(rate_percent<75)rate_percent=75;
 if(rate_percent>125)rate_percent=125;
 const uint64_t elapsed=base_ms>=anchor_base_ms?
        (uint64_t)(base_ms-anchor_base_ms):0U;
 const uint64_t result=(uint64_t)anchor_song_ms+
                       (elapsed*(uint64_t)rate_percent)/100U;
 return result>UINT32_MAX?UINT32_MAX:(uint32_t)result;
}
