#include "lyric_clock.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    lf_lyric_clock_t c={0};
    assert(!c.started && !c.running);
    assert(lf_lyric_clock_position(&c,5000)==0);
    lf_lyric_clock_pause(&c,6000);
    lf_lyric_clock_resume(&c,6000);
    assert(!c.running);
    lf_lyric_clock_start(&c,5000);
    assert(c.started && c.running);
    assert(lf_lyric_clock_position(&c,5000)==0);
    assert(lf_lyric_clock_position(&c,6050)==1050);
    lf_lyric_clock_adjust(&c,2000);
    assert(lf_lyric_clock_position(&c,6050)==3050);
    lf_lyric_clock_adjust(&c,-5000);
    assert(lf_lyric_clock_position(&c,6050)==0);
    lf_lyric_clock_pause(&c,7000);
    assert(!c.running && lf_lyric_clock_position(&c,11000)==0);
    lf_lyric_clock_resume(&c,11000);
    assert(c.running && lf_lyric_clock_position(&c,13000)==1000);
    lf_lyric_clock_adjust(&c,200000);
    assert(c.correction_ms==10000);
    assert(lf_lyric_clock_position(&c,13000)==14000);
    lf_lyric_clock_adjust(&c,-200000);
    assert(c.correction_ms==-10000);
    lf_lyric_clock_start(&c,19000);
    assert(c.correction_ms==0 && lf_lyric_clock_position(&c,19000)==0);
    const uint32_t marks[]={6000,10000,11000,14000,14000,19000};
    assert(lf_lyric_clock_line_at(marks,6,0)==-1);
    assert(lf_lyric_clock_line_at(marks,6,5999)==-1);
    assert(lf_lyric_clock_line_at(marks,6,6000)==0);
    assert(lf_lyric_clock_line_at(marks,6,13000)==2);
    assert(lf_lyric_clock_line_at(marks,6,14000)==4);
    assert(lf_lyric_clock_line_at(marks,6,999999)==5);
    assert(lf_lyric_clock_line_at(NULL,0,5000)==-1);
    assert(lf_lyric_clock_line_at(marks,0,5000)==-1);
    assert(lf_lyric_clock_line_at(NULL,6,5000)==-1);
    lf_lyric_clock_pause(&c,19000);
    lf_lyric_clock_resume(&c,19000);
    assert(lf_lyric_clock_position(&c,UINT64_MAX)==UINT32_MAX);
    puts("PASS: manual lyric clock start/restart, pause/resume, +/-2s calibration, clamp and binary timed-line selection");
    return 0;
}
