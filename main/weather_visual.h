#pragma once
#include "lvgl.h"
#include "weather.h"
/* Build/refresh vector weather artwork from the background-fetched snapshot. */
void lf_weather_visual_build(lv_obj_t *parent);
void lf_weather_visual_update(const lf_weather_view_t *weather);
void lf_weather_visual_clear(void);
