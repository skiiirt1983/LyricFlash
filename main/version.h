#pragma once
/* LyricFlash 0.8.75 private alpha: correct physical edge swipe mapping after upstream LVGL audit.
 * retaining colour swatches, one Clock reset and separate
 * Time/AMPM, Battery, Weather and Date colours on six faces.
 * Additive schema 16 preserves NVS and prior users' clock customizations.
 * New open-licensed clock fonts remain monochrome on embedded LVGL.
 * Do not install unvalidated 0.8.74 (swipe direction reversed); 0.8.73 and older preserved. */
#define LF_VERSION "0.8.75-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.75"
/* LVGL low-level scroll checks confirm end flags use opposite sign from
 * finger motion. Preserve 0.8.73 wake guards and all audio/OTA behavior.
 * On-device edge swipes and wake must still be physically validated. */
