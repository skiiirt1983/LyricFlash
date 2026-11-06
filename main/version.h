#pragma once
/* LyricFlash private alpha: skip a second full-page repaint only when LVGL confirms the destination frame; retain Copy Log and wake safeguards.
 * retaining colour swatches, one Clock reset and separate
 * Time/AMPM, Battery, Weather and Date colours on six faces.
 * Additive schema 16 preserves NVS and prior users' clock customizations.
 * New open-licensed clock fonts remain monochrome on embedded LVGL.
 * Supersede 0.8.74/0.8.75 dynamic scroll restrictions; preserve 0.8.73 and older. */
#define LF_VERSION "0.8.102-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.102"
/* Creator/maintainer skiiirt1983; no changes to settings, credentials or OTA partitions. */
/* 0.8.102: -O2, 64KB data cache, TLS in PSRAM, dual LVGL draw units, native clock font sizes (no transform_scale layers), instant screen changes, temporary FPS/CPU meter. */
/* 0.8.101: performance rework. Tear mode NONE (LVGL partial render), double-buffered 24-row internal-SRAM strips sent by SPI DMA directly (no PSRAM full-frame render), 30 Hz clock sweep. Supersedes 0.8.100 TE-sync full-frame rendering. */
/* 0.8.100 (superseded): restore QSPI TE synchronization using esp_lvgl_adapter 0.6.0 buffer-switch fixes and a bounded 200 ms GPIO-TE TX-done wait. Retain 8-row DMA staging and 32-row transfer limit. Hardware verification pending. */
/* 0.8.75 blocked return swipe on-device despite passing source tests.
 * Restore LV_DIR_HOR unconditionally while retaining no-elastic/no-chain,
 * touch activity, wake cover and 0.8.72 clock pause. Host preflight passed; hardware test pending. */
