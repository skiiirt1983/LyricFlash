#pragma once
/* LyricFlash private alpha: LVGL draw and flush profiling with guarded Home updates, retaining safe touch wake.
 * retaining colour swatches, one Clock reset and separate
 * Time/AMPM, Battery, Weather and Date colours on six faces.
 * Additive schema 16 preserves NVS and prior users' clock customizations.
 * New open-licensed clock fonts remain monochrome on embedded LVGL.
 * Supersede 0.8.74/0.8.75 dynamic scroll restrictions; preserve 0.8.73 and older. */
#define LF_VERSION "0.8.86-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.86"
/* 0.8.86 branding and creator credits preflight passed; 0.8.82 battery test remains in progress. */
/* 0.8.86 source/native preflight passed; 8-row DMA staging requires physical QA. */
/* 0.8.75 blocked return swipe on-device despite passing source tests.
 * Restore LV_DIR_HOR unconditionally while retaining no-elastic/no-chain,
 * touch activity, wake cover and 0.8.72 clock pause. Host preflight passed; hardware test pending. */
