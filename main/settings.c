#include "settings.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>

static uint8_t in_range(uint8_t n, uint8_t lo, uint8_t hi, uint8_t def)
{
    return n >= lo && n <= hi ? n : def;
}

void lf_settings_defaults(lf_settings_t *s)
{
    *s = (lf_settings_t){
        .brightness = 55,
        .scroll_speed = 70,
        .font_size = 1,
        .system_font_size = 2,
        .sleep_seconds = 60,
        .deep_sleep_minutes = 0, /* opt in only after testing GPIO11 touch wake */
        .text_color = 0x86E3AE,
        .background_color = 0x000000,
        .accent_color = 0x86E3AE,
        .timing_offset_ms = 0, /* neutral baseline; manual tuning saved separately */
        .line_spacing_px = 5,
        .clock_face = 0,
        .basic_font_style = 0,
        .basic_font_size = 60,
        .basic_time_x = 0, .basic_time_y = -6,
        .basic_battery_x = 0, .basic_battery_y = -160,
        .basic_date_x = 0, .basic_date_y = 70,
        .basic_weather_x = 0, .basic_weather_y = 135,
        .basic_battery_size = 85, .basic_date_size = 70,
        .basic_weather_size = 70,
        .basic_ring_mode = 1,
        .basic_show_date = 1,
        .basic_weather = 0,
        .basic_background = 0x030810,
        .basic_foreground = 0xF0F8FF,
        .basic_ring_a = 0x268BFF,
        .basic_ring_b = 0x8E48FF,
        .basic_ring_c = 0x20C9D4,
        .show_progress = 1,
        .charging_color = 0x42DC89
    };
    static const uint32_t b[6]={0x070F14,0x030815,0x080C13,0xA7C7A0,0xC8BB94,0x030810};
    static const uint32_t f[6]={0xD8ECE9,0xEDF7FF,0xE5EDF8,0x162C1F,0x332719,0xF0F8FF};
    static const uint32_t a[6]={0x40C0E6,0x7DCEFF,0x6DBFFF,0x364641,0xA88E55,0x268BFF};
    static const int32_t battery_y[6]={-137,-146,-109,-167,-172,-160};
    static const int32_t date_x[6]={99,0,0,0,0,0};
    static const int32_t date_y[6]={-2,72,106,-53,135,70};
    for (unsigned i=0;i<6;i++){
        s->face_battery_x[i]=0;s->face_battery_y[i]=battery_y[i];
        s->face_battery_size[i]=i==5?85:60;
        s->face_show_percent[i]=1;s->face_show_icon[i]=1;
        s->face_date_format[i]=0;
        s->face_show_weather[i]=0;
        s->face_weather_x[i]=0;s->face_weather_y[i]=i==5?135:138;
        s->face_weather_size[i]=i==5?70:60;
        s->face_date_x[i]=date_x[i];s->face_date_y[i]=date_y[i];
        s->face_date_size[i]=i==5?70:60;
        s->face_background[i]=b[i];s->face_foreground[i]=f[i];
        s->face_accent[i]=a[i];
        s->face_time_color[i]=f[i];s->face_battery_color[i]=a[i];
        s->face_weather_color[i]=f[i];s->face_date_color[i]=i==0?0x132B36:f[i];
        s->face_show_date[i]=1;
        s->face_show_seconds[i]=1;s->face_show_battery[i]=1;s->face_24h[i]=0;
    }
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
    /* One-time migration: the OLD 35% rate becomes the NEW 100% cap.
       Convert old persisted rate to the closest new equivalent without
       overwriting NVS until the next regular save. */
    if (schema > 0 && schema < 6) {
        uint8_t old_speed = s->scroll_speed;
        if (old_speed <= 35) {
            float prior_px = 18.0f + 1.3f * (float)old_speed;
            int mapped = (int)((prior_px - 3.0f) / 0.605f + 0.5f);
            s->scroll_speed = (uint8_t)(mapped > 100 ? 100 : (mapped < 10 ? 10 : mapped));
        } else {
            s->scroll_speed = 100;
        }
    }
    (void)nvs_get_u8(h, "font", &s->font_size);
    (void)nvs_get_u8(h, "uifont", &s->system_font_size);
    (void)nvs_get_u8(h, "timeout", &s->sleep_seconds);
    (void)nvs_get_u8(h, "deepmins", &s->deep_sleep_minutes);
    (void)nvs_get_u32(h, "textcolor", &s->text_color);
    (void)nvs_get_u32(h, "bgcolor", &s->background_color);
    (void)nvs_get_u32(h, "accent", &s->accent_color);
    (void)nvs_get_i32(h, "offset_ms", &s->timing_offset_ms);
    /* Firmware 0.8.43-45 applied -2000ms as a saved default. Feedback on
       0.8.45 confirmed this now makes lyrics two seconds late. Migrate
       ONLY that legacy value; retain all other personalized corrections.
       Once saved with schema 13 an intentional -2000ms stays untouched. */
    if(schema<13 && s->timing_offset_ms==-2000)
        s->timing_offset_ms=0;
    (void)nvs_get_u8(h, "spacing", &s->line_spacing_px);
    (void)nvs_get_u8(h, "progress", &s->show_progress);
    (void)nvs_get_u32(h, "chargeclr", &s->charging_color);
    (void)nvs_get_u8(h, "clockface", &s->clock_face);
    (void)nvs_get_u8(h, "basicfont", &s->basic_font_style);
    (void)nvs_get_u8(h, "basicsize", &s->basic_font_size);
    (void)nvs_get_i32(h, "bt_x", &s->basic_time_x);
    (void)nvs_get_i32(h, "bt_y", &s->basic_time_y);
    (void)nvs_get_i32(h, "bb_x", &s->basic_battery_x);
    (void)nvs_get_i32(h, "bb_y", &s->basic_battery_y);
    (void)nvs_get_i32(h, "bd_x", &s->basic_date_x);
    (void)nvs_get_i32(h, "bd_y", &s->basic_date_y);
    (void)nvs_get_i32(h, "bw_x", &s->basic_weather_x);
    (void)nvs_get_i32(h, "bw_y", &s->basic_weather_y);
    (void)nvs_get_u8(h, "bb_size", &s->basic_battery_size);
    (void)nvs_get_u8(h, "bd_size", &s->basic_date_size);
    (void)nvs_get_u8(h, "bw_size", &s->basic_weather_size);
    (void)nvs_get_u8(h, "ringmode", &s->basic_ring_mode);
    (void)nvs_get_u8(h, "basicdate", &s->basic_show_date);
    (void)nvs_get_u8(h, "basicweather", &s->basic_weather);
    (void)nvs_get_u32(h, "basicbg", &s->basic_background);
    (void)nvs_get_u32(h, "basicfg", &s->basic_foreground);
    (void)nvs_get_u32(h, "ring0", &s->basic_ring_a);
    (void)nvs_get_u32(h, "ring1", &s->basic_ring_b);
    (void)nvs_get_u32(h, "ring2", &s->basic_ring_c);
    if(schema >= 7) for(unsigned i=0;i<6;i++) {
        char key[13];
        snprintf(key,sizeof key,"f%ubg",i);(void)nvs_get_u32(h,key,&s->face_background[i]);
        snprintf(key,sizeof key,"f%ufg",i);(void)nvs_get_u32(h,key,&s->face_foreground[i]);
        snprintf(key,sizeof key,"f%uac",i);(void)nvs_get_u32(h,key,&s->face_accent[i]);
        if(schema>=16){
            snprintf(key,sizeof key,"f%utc",i);(void)nvs_get_u32(h,key,&s->face_time_color[i]);
            snprintf(key,sizeof key,"f%ubc",i);(void)nvs_get_u32(h,key,&s->face_battery_color[i]);
            snprintf(key,sizeof key,"f%uwc",i);(void)nvs_get_u32(h,key,&s->face_weather_color[i]);
            snprintf(key,sizeof key,"f%udc",i);(void)nvs_get_u32(h,key,&s->face_date_color[i]);
        }
        snprintf(key,sizeof key,"f%udt",i);(void)nvs_get_u8(h,key,&s->face_show_date[i]);
        snprintf(key,sizeof key,"f%usc",i);(void)nvs_get_u8(h,key,&s->face_show_seconds[i]);
        snprintf(key,sizeof key,"f%ubt",i);(void)nvs_get_u8(h,key,&s->face_show_battery[i]);
        snprintf(key,sizeof key,"f%u24",i);(void)nvs_get_u8(h,key,&s->face_24h[i]);
        snprintf(key,sizeof key,"f%ubx",i);(void)nvs_get_i32(h,key,&s->face_battery_x[i]);
        snprintf(key,sizeof key,"f%uby",i);(void)nvs_get_i32(h,key,&s->face_battery_y[i]);
        snprintf(key,sizeof key,"f%ubs",i);(void)nvs_get_u8(h,key,&s->face_battery_size[i]);
        snprintf(key,sizeof key,"f%upc",i);(void)nvs_get_u8(h,key,&s->face_show_percent[i]);
        snprintf(key,sizeof key,"f%uic",i);(void)nvs_get_u8(h,key,&s->face_show_icon[i]);
        snprintf(key,sizeof key,"f%udf",i);(void)nvs_get_u8(h,key,&s->face_date_format[i]);
        snprintf(key,sizeof key,"f%uwv",i);(void)nvs_get_u8(h,key,&s->face_show_weather[i]);
        snprintf(key,sizeof key,"f%uwx",i);(void)nvs_get_i32(h,key,&s->face_weather_x[i]);
        snprintf(key,sizeof key,"f%uwy",i);(void)nvs_get_i32(h,key,&s->face_weather_y[i]);
        snprintf(key,sizeof key,"f%uws",i);(void)nvs_get_u8(h,key,&s->face_weather_size[i]);
        snprintf(key,sizeof key,"f%udx",i);(void)nvs_get_i32(h,key,&s->face_date_x[i]);
        snprintf(key,sizeof key,"f%udy",i);(void)nvs_get_i32(h,key,&s->face_date_y[i]);
        snprintf(key,sizeof key,"f%udz",i);(void)nvs_get_u8(h,key,&s->face_date_size[i]);
    }
    nvs_close(h);
    /* New per-face controls must inherit preexisting Basic customization. */
    if(schema<14){
        s->face_battery_size[5]=s->basic_battery_size;
        s->face_date_size[5]=s->basic_date_size;
        s->face_date_x[5]=s->basic_date_x;
        s->face_date_y[5]=s->basic_date_y;
    }

    if(schema<16){
        /* Migrating older releases must inherit each face's saved colours,
           rather than resetting the user's custom palette. */
        for(unsigned i=0;i<6;i++){
            s->face_time_color[i]=s->face_foreground[i];
            s->face_battery_color[i]=s->face_accent[i];
            s->face_weather_color[i]=s->face_foreground[i];
            s->face_date_color[i]=i==0?0x132B36:s->face_foreground[i];
        }
    }
    if(schema<15){
        s->face_show_weather[5]=s->basic_weather;
        s->face_weather_x[5]=s->basic_weather_x;
        s->face_weather_y[5]=s->basic_weather_y;
        s->face_weather_size[5]=s->basic_weather_size;
    }
    s->brightness = in_range(s->brightness, 5, 100, 55);
    s->scroll_speed = in_range(s->scroll_speed, 0, 100, 70);
    s->font_size = in_range(s->font_size, 0, 2, 1);
    s->system_font_size = in_range(s->system_font_size, 0, 2, 2);
    if (s->sleep_seconds != 0 && s->sleep_seconds != 15 &&
        s->sleep_seconds != 30 && s->sleep_seconds != 60) s->sleep_seconds = 60;
    if (s->deep_sleep_minutes != 0 && s->deep_sleep_minutes != 5 &&
        s->deep_sleep_minutes != 15 && s->deep_sleep_minutes != 30 &&
        s->deep_sleep_minutes != 60) s->deep_sleep_minutes = 0;
    s->text_color &= 0xFFFFFF;
    s->background_color &= 0xFFFFFF;
    s->accent_color &= 0xFFFFFF;
    if (s->timing_offset_ms < -10000 || s->timing_offset_ms > 10000) s->timing_offset_ms = 0;
    s->line_spacing_px = in_range(s->line_spacing_px, 0, 24, 5);
    s->show_progress = in_range(s->show_progress, 0, 1, 1);
    s->charging_color &= 0xFFFFFF;
    s->clock_face = in_range(s->clock_face, 0, 5, 0);
    s->basic_font_style = in_range(s->basic_font_style, 0, 39, 0);
    s->basic_font_size = in_range(s->basic_font_size, 0, 200, 60);
    /* Per-element offsets remain bounded inside the AMOLED circle. */
    if(s->basic_time_x<-100||s->basic_time_x>100)s->basic_time_x=0;
    if(s->basic_time_y<-150||s->basic_time_y>150)s->basic_time_y=-6;
    if(s->basic_battery_x<-150||s->basic_battery_x>150)s->basic_battery_x=0;
    if(s->basic_battery_y<-180||s->basic_battery_y>180)s->basic_battery_y=-160;
    if(s->basic_date_x<-150||s->basic_date_x>150)s->basic_date_x=0;
    if(s->basic_date_y<-180||s->basic_date_y>180)s->basic_date_y=70;
    if(s->basic_weather_x<-150||s->basic_weather_x>150)s->basic_weather_x=0;
    if(s->basic_weather_y<-180||s->basic_weather_y>180)s->basic_weather_y=135;
    s->basic_battery_size=in_range(s->basic_battery_size,0,200,85);
    s->basic_date_size=in_range(s->basic_date_size,0,200,70);
    s->basic_weather_size=in_range(s->basic_weather_size,0,200,70);
    s->basic_ring_mode = in_range(s->basic_ring_mode, 0, 2, 1);
    s->basic_show_date = in_range(s->basic_show_date, 0, 1, 1);
    s->basic_weather = in_range(s->basic_weather, 0, 1, 0);
    s->basic_background &= 0xFFFFFF;
    s->basic_foreground &= 0xFFFFFF;
    s->basic_ring_a &= 0xFFFFFF;
    s->basic_ring_b &= 0xFFFFFF;
    s->basic_ring_c &= 0xFFFFFF;
    for(unsigned i=0;i<6;i++){
        s->face_background[i]&=0xFFFFFF;
        s->face_foreground[i]&=0xFFFFFF;
        s->face_accent[i]&=0xFFFFFF;
        s->face_show_date[i]=in_range(s->face_show_date[i],0,1,1);
        s->face_show_seconds[i]=in_range(s->face_show_seconds[i],0,1,1);
        s->face_show_battery[i]=in_range(s->face_show_battery[i],0,1,1);
        s->face_24h[i]=in_range(s->face_24h[i],0,1,0);
        s->face_battery_size[i]=in_range(s->face_battery_size[i],0,200,i==5?85:60);
         s->face_show_percent[i]=in_range(s->face_show_percent[i],0,1,1);
        s->face_show_icon[i]=in_range(s->face_show_icon[i],0,1,1);
        s->face_date_format[i]=in_range(s->face_date_format[i],0,5,0);
        s->face_show_weather[i]=in_range(s->face_show_weather[i],0,1,0);
        s->face_weather_size[i]=in_range(s->face_weather_size[i],0,200,60);
        if(s->face_weather_x[i]<-150||s->face_weather_x[i]>150)s->face_weather_x[i]=0;
        if(s->face_weather_y[i]<-180||s->face_weather_y[i]>180)s->face_weather_y[i]=138;
        s->face_date_size[i]=in_range(s->face_date_size[i],0,200,i==5?70:60);
        if(s->face_date_x[i]<-150||s->face_date_x[i]>150)s->face_date_x[i]=i==0?99:0;
        if(s->face_date_y[i]<-180||s->face_date_y[i]>180)
            s->face_date_y[i]=i==0?-2:i==1?72:i==2?106:i==3?-53:i==4?135:70;
        if(s->face_battery_x[i]<-150||s->face_battery_x[i]>150)s->face_battery_x[i]=0;
        if(s->face_battery_y[i]<-180||s->face_battery_y[i]>180)
            s->face_battery_y[i]=i==0?-137:i==1?-146:i==2?-109:i==3?-167:i==4?-172:-160;
    }
    /* For older settings, Basic face still uses its original saved palette. */
    if(schema<7){s->face_background[5]=s->basic_background;
                 s->face_foreground[5]=s->basic_foreground;
                 s->face_show_date[5]=s->basic_show_date;}
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
    TRY_NVS(nvs_set_u8(h, "deepmins", s->deep_sleep_minutes));
    TRY_NVS(nvs_set_u32(h, "textcolor", s->text_color));
    TRY_NVS(nvs_set_u32(h, "bgcolor", s->background_color));
    TRY_NVS(nvs_set_u32(h, "accent", s->accent_color));
    TRY_NVS(nvs_set_i32(h, "offset_ms", s->timing_offset_ms));
    TRY_NVS(nvs_set_u8(h, "spacing", s->line_spacing_px));
    TRY_NVS(nvs_set_u8(h, "progress", s->show_progress));
    TRY_NVS(nvs_set_u32(h, "chargeclr", s->charging_color));
    TRY_NVS(nvs_set_u8(h, "clockface", s->clock_face));
    TRY_NVS(nvs_set_u8(h, "basicfont", s->basic_font_style));
    TRY_NVS(nvs_set_u8(h, "basicsize", s->basic_font_size));
    TRY_NVS(nvs_set_i32(h, "bt_x", s->basic_time_x));
    TRY_NVS(nvs_set_i32(h, "bt_y", s->basic_time_y));
    TRY_NVS(nvs_set_i32(h, "bb_x", s->basic_battery_x));
    TRY_NVS(nvs_set_i32(h, "bb_y", s->basic_battery_y));
    TRY_NVS(nvs_set_i32(h, "bd_x", s->basic_date_x));
    TRY_NVS(nvs_set_i32(h, "bd_y", s->basic_date_y));
    TRY_NVS(nvs_set_i32(h, "bw_x", s->basic_weather_x));
    TRY_NVS(nvs_set_i32(h, "bw_y", s->basic_weather_y));
    TRY_NVS(nvs_set_u8(h, "bb_size", s->basic_battery_size));
    TRY_NVS(nvs_set_u8(h, "bd_size", s->basic_date_size));
    TRY_NVS(nvs_set_u8(h, "bw_size", s->basic_weather_size));
    TRY_NVS(nvs_set_u8(h, "ringmode", s->basic_ring_mode));
    TRY_NVS(nvs_set_u8(h, "basicdate", s->basic_show_date));
    TRY_NVS(nvs_set_u8(h, "basicweather", s->basic_weather));
    TRY_NVS(nvs_set_u32(h, "basicbg", s->basic_background));
    TRY_NVS(nvs_set_u32(h, "basicfg", s->basic_foreground));
    TRY_NVS(nvs_set_u32(h, "ring0", s->basic_ring_a));
    TRY_NVS(nvs_set_u32(h, "ring1", s->basic_ring_b));
    TRY_NVS(nvs_set_u32(h, "ring2", s->basic_ring_c));
    for(unsigned i=0;i<6;i++){
        char key[13];
        snprintf(key,sizeof key,"f%ubg",i);TRY_NVS(nvs_set_u32(h,key,s->face_background[i]));
        snprintf(key,sizeof key,"f%ufg",i);TRY_NVS(nvs_set_u32(h,key,s->face_foreground[i]));
        snprintf(key,sizeof key,"f%uac",i);TRY_NVS(nvs_set_u32(h,key,s->face_accent[i]));
        snprintf(key,sizeof key,"f%utc",i);TRY_NVS(nvs_set_u32(h,key,s->face_time_color[i]));
        snprintf(key,sizeof key,"f%ubc",i);TRY_NVS(nvs_set_u32(h,key,s->face_battery_color[i]));
        snprintf(key,sizeof key,"f%uwc",i);TRY_NVS(nvs_set_u32(h,key,s->face_weather_color[i]));
        snprintf(key,sizeof key,"f%udc",i);TRY_NVS(nvs_set_u32(h,key,s->face_date_color[i]));
        snprintf(key,sizeof key,"f%udt",i);TRY_NVS(nvs_set_u8(h,key,s->face_show_date[i]));
        snprintf(key,sizeof key,"f%usc",i);TRY_NVS(nvs_set_u8(h,key,s->face_show_seconds[i]));
        snprintf(key,sizeof key,"f%ubt",i);TRY_NVS(nvs_set_u8(h,key,s->face_show_battery[i]));
        snprintf(key,sizeof key,"f%u24",i);TRY_NVS(nvs_set_u8(h,key,s->face_24h[i]));
        snprintf(key,sizeof key,"f%ubx",i);TRY_NVS(nvs_set_i32(h,key,s->face_battery_x[i]));
        snprintf(key,sizeof key,"f%uby",i);TRY_NVS(nvs_set_i32(h,key,s->face_battery_y[i]));
        snprintf(key,sizeof key,"f%ubs",i);TRY_NVS(nvs_set_u8(h,key,s->face_battery_size[i]));
        snprintf(key,sizeof key,"f%upc",i);TRY_NVS(nvs_set_u8(h,key,s->face_show_percent[i]));
        snprintf(key,sizeof key,"f%uic",i);TRY_NVS(nvs_set_u8(h,key,s->face_show_icon[i]));
        snprintf(key,sizeof key,"f%udf",i);TRY_NVS(nvs_set_u8(h,key,s->face_date_format[i]));
        snprintf(key,sizeof key,"f%uwv",i);TRY_NVS(nvs_set_u8(h,key,s->face_show_weather[i]));
        snprintf(key,sizeof key,"f%uwx",i);TRY_NVS(nvs_set_i32(h,key,s->face_weather_x[i]));
        snprintf(key,sizeof key,"f%uwy",i);TRY_NVS(nvs_set_i32(h,key,s->face_weather_y[i]));
        snprintf(key,sizeof key,"f%uws",i);TRY_NVS(nvs_set_u8(h,key,s->face_weather_size[i]));
        snprintf(key,sizeof key,"f%udx",i);TRY_NVS(nvs_set_i32(h,key,s->face_date_x[i]));
        snprintf(key,sizeof key,"f%udy",i);TRY_NVS(nvs_set_i32(h,key,s->face_date_y[i]));
        snprintf(key,sizeof key,"f%udz",i);TRY_NVS(nvs_set_u8(h,key,s->face_date_size[i]));
    }
    TRY_NVS(nvs_set_u32(h, "schema", LF_CONFIG_SCHEMA));
    err = nvs_commit(h);
close_nvs:
    nvs_close(h);
    return err;
#undef TRY_NVS
}
