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
