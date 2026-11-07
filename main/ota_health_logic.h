#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Pure, host-testable acceptance rule for a pending OTA image. */
typedef struct {
    uint32_t ui_tick_age_ms;
    uint32_t render_count;
    uint32_t flush_finishes;
    uint32_t refresh_ready;
    uint32_t free_heap_bytes;
} lf_ota_health_sample_t;

bool lf_ota_health_sample_ok(const lf_ota_health_sample_t *sample);
