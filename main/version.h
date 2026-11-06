#pragma once
/* LyricFlash private alpha: skip a second full-page repaint only when LVGL confirms the destination frame; retain Copy Log and wake safeguards.
 * retaining colour swatches, one Clock reset and separate
 * Time/AMPM, Battery, Weather and Date colours on six faces.
 * Additive schema 16 preserves NVS and prior users' clock customizations.
 * New open-licensed clock fonts remain monochrome on embedded LVGL.
 * Supersede 0.8.74/0.8.75 dynamic scroll restrictions; preserve 0.8.73 and older. */
#define LF_VERSION "0.8.99-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.99"
/* Creator/maintainer skiiirt1983; no changes to settings, credentials or OTA partitions. */
/* 0.8.99: isolated 32-row PSRAM LVGL draw-buffer experiment atop hardware-usable 0.8.95. 8-row DMA staging, original chunk threshold, network and menus unchanged. Hardware stability NOT verified. */
/* 0.8.75 blocked return swipe on-device despite passing source tests.
 * Restore LV_DIR_HOR unconditionally while retaining no-elastic/no-chain,
 * touch activity, wake cover and 0.8.72 clock pause. Host preflight passed; hardware test pending. */
