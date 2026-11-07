#include "lyric_clock.h"
#include <limits.h>

static uint32_t unadjusted(const lf_lyric_clock_t *c,uint64_t now)
{
    if(!c || !c->started)return 0;
    uint64_t elapsed=c->elapsed_ms;
    if(c->running && now>=c->origin_ms)elapsed+=now-c->origin_ms;
    return elapsed>UINT32_MAX?UINT32_MAX:(uint32_t)elapsed;
}
void lf_lyric_clock_start(lf_lyric_clock_t *c,uint64_t now)
{
    if(!c)return;
    c->origin_ms=now;c->elapsed_ms=0;c->correction_ms=0;
    c->started=true;c->running=true;
}
void lf_lyric_clock_pause(lf_lyric_clock_t *c,uint64_t now)
{
    if(!c||!c->started||!c->running)return;
    c->elapsed_ms=unadjusted(c,now);
    c->running=false;
}
void lf_lyric_clock_resume(lf_lyric_clock_t *c,uint64_t now)
{
    if(!c||!c->started||c->running)return;
    c->origin_ms=now;c->running=true;
}
void lf_lyric_clock_adjust(lf_lyric_clock_t *c,int32_t delta_ms)
{
    if(!c)return;
    int64_t v=(int64_t)c->correction_ms+delta_ms;
    if(v>10000)v=10000;
    if(v< -10000)v= -10000;
    c->correction_ms=(int32_t)v;
}
uint32_t lf_lyric_clock_position(const lf_lyric_clock_t *c,uint64_t now)
{
    if(!c||!c->started)return 0;
    int64_t pos=(int64_t)unadjusted(c,now)+c->correction_ms;
    if(pos<=0)return 0;
    return pos>UINT32_MAX?UINT32_MAX:(uint32_t)pos;
}
int lf_lyric_clock_line_at(const uint32_t *timestamps,size_t n,uint32_t pos)
{
    if(!timestamps||n==0||pos<timestamps[0])return -1;
    size_t lo=0,hi=n;
    while(lo<hi){
        size_t mid=lo+(hi-lo)/2;
        if(timestamps[mid]<=pos)lo=mid+1;
        else hi=mid;
    }
    return (int)(lo-1);
}
