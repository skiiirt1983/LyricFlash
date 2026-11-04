#pragma once
/* LyricFlash 0.8.70 private alpha: suspend 16ms clock sweep only while screen off.
 * retaining colour swatches, one Clock reset and separate
 * Time/AMPM, Battery, Weather and Date colours on six faces.
 * Additive schema 16 preserves NVS and prior users' clock customizations.
 * New open-licensed clock fonts remain monochrome on embedded LVGL.
 * Working 0.8.69, 0.8.68 and 0.8.67 remain published unchanged. */
#define LF_VERSION "0.8.70-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.70"
/* Waveform-only source/native regression preflight passed.
 * The previous 0.8.68 firmware remains unchanged for safe fallback;
 * physical waveform-height validation remains pending after compilation. */
