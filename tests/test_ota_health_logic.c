#include "ota_health_logic.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

int main(void)
{
    lf_ota_health_sample_t s={.ui_tick_age_ms=50,.render_count=6,
                              .flush_finishes=6,.refresh_ready=6,
                              .free_heap_bytes=32000};
    assert(lf_ota_health_sample_ok(&s));
    s.ui_tick_age_ms=1500;assert(lf_ota_health_sample_ok(&s));
    s.ui_tick_age_ms=1501;assert(!lf_ota_health_sample_ok(&s));
    s.ui_tick_age_ms=0;s.render_count=0;assert(!lf_ota_health_sample_ok(&s));
    s.render_count=5;s.flush_finishes=0;assert(!lf_ota_health_sample_ok(&s));
    s.flush_finishes=3;s.refresh_ready=0;assert(!lf_ota_health_sample_ok(&s));
    s.refresh_ready=3;s.free_heap_bytes=16383;assert(!lf_ota_health_sample_ok(&s));
    s.free_heap_bytes=16384;assert(lf_ota_health_sample_ok(&s));
    assert(!lf_ota_health_sample_ok(NULL));
    puts("PASS: OTA health checks heartbeat, render, flush and heap safety");
    return 0;
}
