#pragma once
#include <stdint.h>
#include "lvgl.h"

/* Round 466 x 466 screen watch-face renderer. Call only from LVGL task. */
void lf_clock_build(lv_obj_t *parent, uint8_t style);
void lf_clock_update(void);
/* Lightweight sub-second sweep, called only while a clock is visible. */
void lf_clock_animate(void);
void lf_clock_clear(void);
const char *lf_clock_style_name(uint8_t style);
