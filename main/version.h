#pragma once
/* LyricFlash 0.8.73 private alpha: protect Device Settings wake and nested touch timeout.
 * retaining colour swatches, one Clock reset and separate
 * Time/AMPM, Battery, Weather and Date colours on six faces.
 * Additive schema 16 preserves NVS and prior users' clock customizations.
 * New open-licensed clock fonts remain monochrome on embedded LVGL.
 * Last tested 0.8.71 and 0.8.69 are preserved; 0.8.72 remains an unchanged fallback. */
#define LF_VERSION "0.8.73-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.73"
/* Keep 0.8.72's power timer and 0.8.71's wake touch capture.
 * Nested Device Settings touch/scroll must not trip an idle screen timeout.
 * All source/native preflight tests passed; hardware wake/release still pending. */
