#pragma once
/* LyricFlash private alpha: skip a second full-page repaint only when LVGL confirms the destination frame; retain Copy Log and wake safeguards.
 * retaining colour swatches, one Clock reset and separate
 * Time/AMPM, Battery, Weather and Date colours on six faces.
 * Additive schema 16 preserves NVS and prior users' clock customizations.
 * New open-licensed clock fonts remain monochrome on embedded LVGL.
 * Supersede 0.8.74/0.8.75 dynamic scroll restrictions; preserve 0.8.73 and older. */
#define LF_VERSION "0.8.113-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.113"
/* Creator/maintainer skiiirt1983; no changes to settings, credentials or OTA partitions. */
/* 0.8.113: direct LVGL/esp_lcd display port (esp_lvgl_adapter bypassed): 2 x 24-row internal DMA strips, ISR-signalled flush completion. */
/* 0.8.112: single Basic clock face (no ornate faces, ring or background colour); transparent pager removes an extra full-screen fill at swipe midpoints. */
/* 0.8.111: fix unsigned idle-time wrap that blanked the screen while touched; LVGL OS none (synchronous rendering, no draw-thread handoffs). */
/* 0.8.110: internal-SRAM double-buffered LVGL strips again (direct DMA, render/transfer overlap); Wi-Fi IRAM optimisations off to fund it; min internal heap in diagnostics. */
/* 0.8.109: screen-off diagnostics (timeout setting, firmware and LVGL idle time, page). */
/* 0.8.108: LVGL circle-mask cache 4 -> 32 radii (benchmark: blank 52 ms, Home 106 ms, Clock 212 ms full frame). */
/* 0.8.107: boot benchmark runs on the LVGL task 1.5 s after start (0.8.106 ran it on app_main's small stack and crashed into OTA rollback). */
/* 0.8.106: restore ~77 KB internal SRAM for song ID: default 32 KB data cache, LVGL strips back in PSRAM with DMA staging. */
/* 0.8.105: screen timeout also honours LVGL's own last-input time (screen was blanking during use). */
/* 0.8.104: boot benchmark (CPU clock, blank/page/midpoint full-frame render times) on Diagnostics and /api/diagnostics; single LVGL draw unit. */
/* 0.8.103: clock hands and sundial shadow redraw only their own pixels (tight lv_line bounds) instead of ~400x400 per sweep step; boot QSPI self-test and LCD buffer/internal RAM readout on Device Diagnostics; mbedTLS dynamic buffers. */
/* 0.8.102: -O2, 64KB data cache, TLS in PSRAM, dual LVGL draw units, native clock font sizes (no transform_scale layers), instant screen changes, temporary FPS/CPU meter. */
/* 0.8.101: performance rework. Tear mode NONE (LVGL partial render), double-buffered 24-row internal-SRAM strips sent by SPI DMA directly (no PSRAM full-frame render), 30 Hz clock sweep. Supersedes 0.8.100 TE-sync full-frame rendering. */
/* 0.8.100 (superseded): restore QSPI TE synchronization using esp_lvgl_adapter 0.6.0 buffer-switch fixes and a bounded 200 ms GPIO-TE TX-done wait. Retain 8-row DMA staging and 32-row transfer limit. Hardware verification pending. */
/* 0.8.75 blocked return swipe on-device despite passing source tests.
 * Restore LV_DIR_HOR unconditionally while retaining no-elastic/no-chain,
 * touch activity, wake cover and 0.8.72 clock pause. Host preflight passed; hardware test pending. */
