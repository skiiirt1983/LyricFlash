#pragma once
/* LyricFlash 0.8.71 private alpha: retain wake touch target until physical finger release.
 * retaining colour swatches, one Clock reset and separate
 * Time/AMPM, Battery, Weather and Date colours on six faces.
 * Additive schema 16 preserves NVS and prior users' clock customizations.
 * New open-licensed clock fonts remain monochrome on embedded LVGL.
 * Working 0.8.69/0.8.68/0.8.67 remain immutable; experimental 0.8.70 stays separate. */
#define LF_VERSION "0.8.71-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.71"
/* Wake-release safety hotfix; preserve the 0.8.69 audio/UI baseline.
 * Hardware wake/release testing is REQUIRED before considering this stable. */
