#pragma once
#include <stdint.h>
#include "lvgl.h"
/* True Google Fonts, converted to small monochrome LVGL glyph sets at build time.
   Indices 24-36 are the requested 13 genuine open-source families. */
const lv_font_t *lf_clock_custom_font(uint8_t style);
