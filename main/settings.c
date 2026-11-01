#include "settings.h"
#include "nvs.h"
#include "nvs_flash.h"

static uint8_t in_range(uint8_t n, uint8_t lo, uint8_t hi, uint8_t def)
{
    return n >= lo && n <= hi ? n : def;
}

void lf_settings_defaults(lf_settings_t *s)
{
    *s = (lf_settings_t){
        .brightness = 55,
        .scroll_speed = 45,
        .font_size = 1,
        .system_font_size = 2,
        .sleep_seconds = 60,
        .text_color = 0x86E3AE,
        .background_color = 0x000000,
        .accent_color = 0x86E3AE,
        .timing_offset_ms = 0,
        .line_spacing_px = 5,
        .clock_face = 0,
        .basic_font_style = 0,
        .basic_font_size = 60,
        .basic_ring_mode = 1,
        .basic_show_date = 1,
        .basic_weather = 0,
        .basic_background = 0x030810,
        .basic_foreground = 0xF0F8FF,
        .basic_ring_a = 0x268BFF,
        .basic_ring_b = 0x8E48FF,
        .basic_ring_c = 0x20C9D4,
        .show_progress = 1
    };
}

esp_err_t lf_settings_load(lf_settings_t *s)
{
    lf_settings_defaults(s);
    nvs_handle_t h;
    esp_err_t err = nvs_open(LF_CONFIG_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;

    // Schema is additive; unknown/new keys are never wiped on an older version.
    uint32_t schema = 0;
    (void)nvs_get_u32(h, "schema", &schema);
    if (schema > LF_CONFIG_SCHEMA) {
        // Older firmware after rollback: read only keys it understands.
    }
    (void)nvs_get_u8(h, "brightness", &s->brightness);
    (void)nvs_get_u8(h, "scroll", &s->scroll_speed);
    (void)nvs_get_u8(h, "font", &s->font_size);
    (void)nvs_get_u8(h, "uifont", &s->system_font_size);
    (void)nvs_get_u8(h, "timeout", &s->sleep_seconds);
    (void)nvs_get_u32(h, "textcolor", &s->text_color);
    (void)nvs_get_u32(h, "bgcolor", &s->background_color);
    (void)nvs_get_u32(h, "accent", &s->accent_color);
    (void)nvs_get_i32(h, "offset_ms", &s->timing_offset_ms);
    (void)nvs_get_u8(h, "spacing", &s->line_spacing_px);
    (void)nvs_get_u8(h, "progress", &s->show_progress);
    (void)nvs_get_u8(h, "clockface", &s->clock_face);
    (void)nvs_get_u8(h, "basicfont", &s->basic_font_style);
    (void)nvs_get_u8(h, "basicsize", &s->basic_font_size);
    (void)nvs_get_u8(h, "ringmode", &s->basic_ring_mode);
    (void)nvs_get_u8(h, "basicdate", &s->basic_show_date);
    (void)nvs_get_u8(h, "basicweather", &s->basic_weather);
    (void)nvs_get_u32(h, "basicbg", &s->basic_background);
    (void)nvs_get_u32(h, "basicfg", &s->basic_foreground);
    (void)nvs_get_u32(h, "ring0", &s->basic_ring_a);
    (void)nvs_get_u32(h, "ring1", &s->basic_ring_b);
    (void)nvs_get_u32(h, "ring2", &s->basic_ring_c);
    nvs_close(h);

    s->brightness = in_range(s->brightness, 5, 100, 55);
    s->scroll_speed = in_range(s->scroll_speed, 10, 100, 45);
    s->font_size = in_range(s->font_size, 0, 2, 1);
    s->system_font_size = in_range(s->system_font_size, 0, 2, 2);
    if (s->sleep_seconds != 0 && s->sleep_seconds != 15 &&
        s->sleep_seconds != 30 && s->sleep_seconds != 60) s->sleep_seconds = 60;
    s->text_color &= 0xFFFFFF;
    s->background_color &= 0xFFFFFF;
    s->accent_color &= 0xFFFFFF;
    if (s->timing_offset_ms < -10000 || s->timing_offset_ms > 10000) s->timing_offset_ms = 0;
    s->line_spacing_px = in_range(s->line_spacing_px, 0, 24, 5);
    s->show_progress = in_range(s->show_progress, 0, 1, 1);
    s->clock_face = in_range(s->clock_face, 0, 5, 0);
    s->basic_font_style = in_range(s->basic_font_style, 0, 4, 0);
    s->basic_font_size = in_range(s->basic_font_size, 0, 100, 60);
    s->basic_ring_mode = in_range(s->basic_ring_mode, 0, 2, 1);
    s->basic_show_date = in_range(s->basic_show_date, 0, 1, 1);
    s->basic_weather = in_range(s->basic_weather, 0, 1, 0);
    s->basic_background &= 0xFFFFFF;
    s->basic_foreground &= 0xFFFFFF;
    s->basic_ring_a &= 0xFFFFFF;
    s->basic_ring_b &= 0xFFFFFF;
    s->basic_ring_c &= 0xFFFFFF;
    return ESP_OK;
}

esp_err_t lf_settings_save(const lf_settings_t *s)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(LF_CONFIG_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
#define TRY_NVS(call) do { err = (call); if (err != ESP_OK) goto close_nvs; } while (0)
    TRY_NVS(nvs_set_u8(h, "brightness", s->brightness));
    TRY_NVS(nvs_set_u8(h, "scroll", s->scroll_speed));
    TRY_NVS(nvs_set_u8(h, "font", s->font_size));
    TRY_NVS(nvs_set_u8(h, "uifont", s->system_font_size));
    TRY_NVS(nvs_set_u8(h, "timeout", s->sleep_seconds));
    TRY_NVS(nvs_set_u32(h, "textcolor", s->text_color));
    TRY_NVS(nvs_set_u32(h, "bgcolor", s->background_color));
    TRY_NVS(nvs_set_u32(h, "accent", s->accent_color));
    TRY_NVS(nvs_set_i32(h, "offset_ms", s->timing_offset_ms));
    TRY_NVS(nvs_set_u8(h, "spacing", s->line_spacing_px));
    TRY_NVS(nvs_set_u8(h, "progress", s->show_progress));
    TRY_NVS(nvs_set_u8(h, "clockface", s->clock_face));
    TRY_NVS(nvs_set_u8(h, "basicfont", s->basic_font_style));
    TRY_NVS(nvs_set_u8(h, "basicsize", s->basic_font_size));
    TRY_NVS(nvs_set_u8(h, "ringmode", s->basic_ring_mode));
    TRY_NVS(nvs_set_u8(h, "basicdate", s->basic_show_date));
    TRY_NVS(nvs_set_u8(h, "basicweather", s->basic_weather));
    TRY_NVS(nvs_set_u32(h, "basicbg", s->basic_background));
    TRY_NVS(nvs_set_u32(h, "basicfg", s->basic_foreground));
    TRY_NVS(nvs_set_u32(h, "ring0", s->basic_ring_a));
    TRY_NVS(nvs_set_u32(h, "ring1", s->basic_ring_b));
    TRY_NVS(nvs_set_u32(h, "ring2", s->basic_ring_c));
    TRY_NVS(nvs_set_u32(h, "schema", LF_CONFIG_SCHEMA));
    err = nvs_commit(h);
close_nvs:
    nvs_close(h);
    return err;
#undef TRY_NVS
}
