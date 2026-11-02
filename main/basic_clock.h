#pragma once
#include "lvgl.h"
#include "settings.h"
/* Render the unbranded Basic face; weather and battery added by clock_face.c */
lv_obj_t *lf_basic_clock_render(lv_obj_t *screen, const lf_settings_t *settings);

/* Retain the Basic ring snapshot across navigation; rebuild only on edit. */
void lf_basic_clock_clear(void);
void lf_basic_clock_prepare(void);
