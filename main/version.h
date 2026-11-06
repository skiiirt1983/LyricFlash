#pragma once
/* LyricFlash private alpha: reduce LVGL draw passes and QSPI DMA chunk submissions; retain the acknowledged redraw guard, Copy Log and wake safeguards.
 * retaining colour swatches, one Clock reset and separate
 * Time/AMPM, Battery, Weather and Date colours on six faces.
 * Additive schema 16 preserves NVS and prior users' clock customizations.
 * New open-licensed clock fonts remain monochrome on embedded LVGL.
 * Supersede 0.8.74/0.8.75 dynamic scroll restrictions; preserve 0.8.73 and older. */
#define LF_VERSION "0.8.96-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.96"
/* Creator/maintainer skiiirt1983; no changes to settings, credentials or OTA partitions. */
/* 0.8.96: 64-row LVGL partial draw buffer, 32-row DMA staging with 8-row fallback.
 * Display smoothness and flash elimination require hardware verification. */
/* 0.8.75 blocked return swipe on-device despite passing source tests.
 * Restore LV_DIR_HOR unconditionally while retaining no-elastic/no-chain,
 * touch activity, wake cover and 0.8.72 clock pause. Host preflight passed; hardware test pending. */
