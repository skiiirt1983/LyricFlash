#pragma once
#include <stdint.h>
#include "esp_err.h"

#define LF_CONFIG_SCHEMA 8U
#define LF_CONFIG_NS "lyric_ui"

typedef struct {
    uint8_t brightness;       // 5...100 %
    uint8_t scroll_speed;     // 10...100%; 100% ~= 63.5 px/sec
    uint8_t font_size;        // lyric size: 0 small, 1 medium, 2 large
    uint8_t system_font_size; // device UI size: 0 18pt, 1 20pt, 2 24pt
    uint8_t sleep_seconds;    // 0 disables timeout; otherwise 15, 30, or 60
    uint32_t text_color;      // 24-bit RRGGBB
    uint32_t background_color; // 24-bit RRGGBB
    uint32_t accent_color;     // 24-bit RRGGBB
    int32_t timing_offset_ms;  // -10000 .. +10000
    uint8_t line_spacing_px;  // 0 .. 24
    uint8_t clock_face;       // 0 diver, 1 midnight, 2 classic, 3 retro LCD, 4 sundial, 5 basic
    /* Basic face is an additional independent, persistently styled option. */
    uint8_t basic_font_style;    // 0 .. 4 typographic presets
    uint8_t basic_font_size;     // 0 .. 100 slider
    uint8_t basic_ring_mode;     // 0 solid, 1 two-tone, 2 three-tone
    uint8_t basic_show_date;     // 0 .. 1
    uint8_t basic_weather;       // 0 .. 1
    uint32_t basic_background;   // 24-bit RGB
    uint32_t basic_foreground;   // 24-bit RGB
    uint32_t basic_ring_a;
    uint32_t basic_ring_b;
    uint32_t basic_ring_c;
    /* Independent editable appearance for all six faces. */
    uint32_t face_background[6];
    uint32_t face_foreground[6];
    uint32_t face_accent[6];
    uint8_t face_show_date[6];
    uint8_t face_show_seconds[6];
    uint8_t face_show_battery[6];
    uint8_t face_24h[6];
    uint32_t charging_color;  // customizable when USB connected
    uint8_t show_progress;    // 0 .. 1
} lf_settings_t;

void lf_settings_defaults(lf_settings_t *s);
esp_err_t lf_settings_load(lf_settings_t *s);
esp_err_t lf_settings_save(const lf_settings_t *s);
