#pragma once
/* LyricFlash 0.8.72 private alpha: battery timer efficiency on top of wake-release fix.
 * retaining colour swatches, one Clock reset and separate
 * Time/AMPM, Battery, Weather and Date colours on six faces.
 * Additive schema 16 preserves NVS and prior users' clock customizations.
 * New open-licensed clock fonts remain monochrome on embedded LVGL.
 * Tested 0.8.71 and prior 0.8.69 remain immutable recovery releases. */
#define LF_VERSION "0.8.72-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.72"
/* Extend the validated 0.8.71 wake-cover lifecycle; deep sleep stays OFF.
 * Host regression preflight passed; physical sleep/wake acceptance remains pending. */
