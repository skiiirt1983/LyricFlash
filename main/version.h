#pragma once
/* LyricFlash 1.0.0 - first stable release for the Waveshare ESP32-S3 Touch AMOLED 1.75.
 * Settings schema 16 is unchanged, so saved Wi-Fi and clock settings carry over. */
#define LF_VERSION "1.0.2"
#define LF_VERSION_LABEL "1.0.2"
/* Creator/maintainer skiiirt1983; no changes to settings, credentials or OTA partitions. */
/* 1.0.2: web Clock Studio saves now apply every setting (time/battery/date/weather colours, battery size,
   icon and %, date format/size/position, weather size/position were validated but dropped); the device's
   Time color and Date buttons now change what the clock draws. */
/* 1.0.1: Device Settings shows the Wi-Fi network (or the open setup network to join) and Song ID (AudD) status;
   Home shows a setup hint while Wi-Fi or the AudD key is missing; setup network is open (no password);
   web clock settings save again (colour fields sent as numbers); web clock preview matches the device
   (black dial, AM/PM beside the digits); device Font button cycles the same 16 fonts as the web page and
   new installs start on Montserrat; Lyrics & Display sliders no longer need unlocking. */
/* 1.0.0: first stable release (0.8.130 code, alpha labels removed).
   0.8.130: time zone set before the first Clock frame (no UTC flash); screen-off power saving (touch and LVGL loop 40 ms while dark); swipe compositor timer paused outside swipes; clock sweep timer 1 s. */
/* 0.8.129: off-screen Clock keeps the current time and its swipe picture refreshes each minute; clock updated before the screen lights on wake (no stale times). */
/* 0.8.128: one combined Clock Settings list (no gap, no Customize sub-screen); clock-face weather fixed (face was built before weather settings loaded); weather refetch on first time sync; AM/PM placed beside the digits. Fixes the 0.8.126/0.8.127 build. */
/* 0.8.127: on-screen FPS/CPU meter removed. */
/* 0.8.126: swipe pictures refresh when the visible page changes (no stale Song ID "Listening" screen during a swipe); snapshot_sync_retakes in diagnostics. */
/* 0.8.125: hidden extra: double-tap the Home logo to wake it up (shake, twist, pulse), send it on a lap around the screen and spring it back into place. */
/* 0.8.124: panel SPI interrupt on core 1; never enter esp_lcd with a transfer outstanding (bounded wait, wedge flag -> immediate restart); stall guard from 2 s after boot; Copy Log works inside the log dialog. */
/* 0.8.123: UI-task breadcrumbs kept across restart; stall guard 4 s, active from 10 s, checked every 1 s. */
/* 0.8.122: touch sampled in its own task (LVGL never blocks on I2C; stale samples read as released); persistent PMIC I2C handle; 6 s LVGL-stall restart guard; input health in diagnostics. */
/* 0.8.121: clock weather shows when either the device menu or Clock Studio switch is on; weather pipeline state in diagnostics. */
/* 0.8.120: settle starts immediately on finger release (no ~125 ms pause); swipe frames start on the panel TE pulse (no tearing). */
/* 0.8.119: no start-up flashes (panel on at zero brightness, black default screen, boot benchmark off); touch/pager state in diagnostics for freeze reports. */
/* 0.8.118: ticketed panel-transfer accounting shared by LVGL and the swipe compositor (no lost/stolen completions); drawing-suspension watchdog. */
/* 0.8.117: balanced flush-completion handoff between LVGL and the swipe compositor (removes the per-swipe 200 ms stall); 4 px scroll grab, 10 ms touch sampling. */
/* 0.8.116: swipe compositor: LVGL drawing suspended during pager motion, frames composed from PSRAM page snapshots and pushed straight to the panel; 240-360 ms ease-out settle. */
/* 0.8.115: swipe snapshots: idle RGB565 page images shown during pager motion, live widgets restored and repainted at commit. */
/* 0.8.114: per-swipe frame statistics (last 8 swipes) in /api/diagnostics. */
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
