#pragma once
#include <stdint.h>
#include "esp_err.h"

#define LF_CONFIG_SCHEMA 2U
#define LF_CONFIG_NS "lyric_ui"

typedef struct {
    uint8_t brightness;       // 5...100 %
    uint8_t scroll_speed;     // 10...100 % (demo speed)
    uint8_t font_size;        // 0 small, 1 medium, 2 large
    uint8_t sleep_seconds;    // 0 disables timeout; otherwise 15, 30, or 60
    uint32_t text_color;      // 24-bit RRGGBB
    uint32_t background_color; // 24-bit RRGGBB
    uint32_t accent_color;     // 24-bit RRGGBB
    int32_t timing_offset_ms;  // -10000 .. +10000
    uint8_t line_spacing_px;  // 0 .. 24
    uint8_t show_progress;    // 0 .. 1
} lf_settings_t;

void lf_settings_defaults(lf_settings_t *s);
esp_err_t lf_settings_load(lf_settings_t *s);
esp_err_t lf_settings_save(const lf_settings_t *s);
