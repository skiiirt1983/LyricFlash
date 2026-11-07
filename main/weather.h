#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Weather config is persisted in NVS independently from UI and Wi-Fi settings.
   Updates happen on a low-priority task, never on the LVGL render thread. */
#define LF_WEATHER_CITY_MAX 47
typedef struct {
    bool enabled;
    uint16_t interval_minutes;   /* 15, 30, 60, or 120 */
    char city[LF_WEATHER_CITY_MAX + 1];
    /* Exact Open-Meteo geocoding selection, saved as integer degrees x 1e5.
       Old text-only configurations remain supported. */
    bool has_coordinates;
    int32_t latitude_e5;
    int32_t longitude_e5;
} lf_weather_config_t;

bool lf_weather_start(void);
void lf_weather_get_config(lf_weather_config_t *out);
bool lf_weather_save_config(const lf_weather_config_t *config);
void lf_weather_get_strip(char *out, size_t max);
void lf_weather_get_detail(char *out, size_t max);
/* Cached numeric weather avoids parsing city names into tiny watchface text. */
bool lf_weather_get_current(int *temp_c, int *wmo_code);

/* Read-only snapshot for the visual weather dashboard (never network calls on LVGL task). */
#define LF_WEATHER_HOURS 4
#define LF_WEATHER_DAYS 3
typedef struct {
    char hour[6];
    int temperature;
    int condition;
} lf_weather_hour_t;
typedef struct {
    int high, low;
    int condition;
} lf_weather_day_t;
typedef struct {
    bool valid;
    char location[24];
    int temperature;
    int condition;
    int feels_like;
    int humidity_pct;
    int wind_kmh;
    int precipitation_tenths_mm;
    lf_weather_hour_t hourly[LF_WEATHER_HOURS];
    lf_weather_day_t daily[LF_WEATHER_DAYS];
    unsigned hour_count;
    unsigned day_count;
} lf_weather_view_t;
void lf_weather_get_view(lf_weather_view_t *out);

/* 0.8.123: weather pipeline state for diagnostics. */
typedef struct {
    bool enabled, valid, last_overflow;
    char city[48];
    char status[68];
    int temperature, last_err, last_http, last_bytes;
    uint32_t fetches, fetch_ok;
} lf_weather_debug_t;
void lf_weather_debug(lf_weather_debug_t *out);
