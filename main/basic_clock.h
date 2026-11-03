#pragma once
#include "lvgl.h"
#include "settings.h"
/* Render the unbranded Basic face; weather and battery added by clock_face.c */
lv_obj_t *lf_basic_clock_render(lv_obj_t *screen, const lf_settings_t *settings);
/* The custom numerals are part of the retained LVGL scene. */
void lf_basic_clock_update_time(const char *value);
void lf_basic_clock_clear(void);
