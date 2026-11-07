#pragma once
#include <stdint.h>
#include "lvgl.h"
/* True Google Fonts, converted to small monochrome LVGL glyph sets at build time.
   Indices 24-36 are the requested 13 genuine open-source families.
   Indices 37-39 are licensable substitutes for the requested system fonts. */
const lv_font_t *lf_clock_custom_font(uint8_t style);
/* Nearest natively generated size (40/56/72/96/120 px) for a style; never scaled. */
const lv_font_t *lf_clock_custom_font_px(uint8_t style, int px);
