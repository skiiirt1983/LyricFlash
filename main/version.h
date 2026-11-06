#pragma once
/* LyricFlash private alpha: LVGL native pager navigation and origin-sensitive clock Back; preserve safe touch wake.
 * retaining colour swatches, one Clock reset and separate
 * Time/AMPM, Battery, Weather and Date colours on six faces.
 * Additive schema 16 preserves NVS and prior users' clock customizations.
 * New open-licensed clock fonts remain monochrome on embedded LVGL.
 * Supersede 0.8.74/0.8.75 dynamic scroll restrictions; preserve 0.8.73 and older. */
#define LF_VERSION "0.8.90-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.90"
/* Creator/maintainer skiiirt1983; no changes to settings, credentials or OTA partitions. */
/* 0.8.90 source/native preflight passed; native snap and Clock return still need physical QA. */
/* 0.8.75 blocked return swipe on-device despite passing source tests.
 * Restore LV_DIR_HOR unconditionally while retaining no-elastic/no-chain,
 * touch activity, wake cover and 0.8.72 clock pause. Host preflight passed; hardware test pending. */
