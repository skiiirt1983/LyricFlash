#pragma once
/* LyricFlash 0.8.80 private alpha: bounded waveform redraw and paced pager/menus.
 * retaining colour swatches, one Clock reset and separate
 * Time/AMPM, Battery, Weather and Date colours on six faces.
 * Additive schema 16 preserves NVS and prior users' clock customizations.
 * New open-licensed clock fonts remain monochrome on embedded LVGL.
 * Supersede 0.8.74/0.8.75 dynamic scroll restrictions; preserve 0.8.73 and older. */
#define LF_VERSION "0.8.80-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.80"
/* Release gate: all host preflight tests passed; 0.8.79 physical hardware test pending. */
/* Host source contracts passed; physical double-tap cancellation and pager-edge QA pending. */
/* 0.8.75 blocked return swipe on-device despite passing source tests.
 * Restore LV_DIR_HOR unconditionally while retaining no-elastic/no-chain,
 * touch activity, wake cover and 0.8.72 clock pause. Host preflight passed; hardware test pending. */
