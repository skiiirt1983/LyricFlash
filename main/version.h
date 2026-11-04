#pragma once
/* LyricFlash 0.8.68 private alpha: delayed OTA confirmation after post-boot UI health.
 * retaining colour swatches, one Clock reset and separate
 * Time/AMPM, Battery, Weather and Date colours on six faces.
 * Additive schema 16 preserves NVS and prior users' clock customizations.
 * New open-licensed clock fonts remain monochrome on embedded LVGL.
 * The hardware-tested 0.8.67 image remains available as rollback. */
#define LF_VERSION "0.8.68-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.68"
/* Source/native preflight passed. The OTA health gate is isolated;
 * on-device validation remains required after verified firmware packaging. */
