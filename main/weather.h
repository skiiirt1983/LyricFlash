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
} lf_weather_config_t;

bool lf_weather_start(void);
void lf_weather_get_config(lf_weather_config_t *out);
bool lf_weather_save_config(const lf_weather_config_t *config);
void lf_weather_get_strip(char *out, size_t max);
void lf_weather_get_detail(char *out, size_t max);
