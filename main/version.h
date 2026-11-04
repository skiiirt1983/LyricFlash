#pragma once
/* LyricFlash 0.8.74 private alpha: constrain Settings/Clock pager edges and guard swipe timeout.
 * retaining colour swatches, one Clock reset and separate
 * Time/AMPM, Battery, Weather and Date colours on six faces.
 * Additive schema 16 preserves NVS and prior users' clock customizations.
 * New open-licensed clock fonts remain monochrome on embedded LVGL.
 * 0.8.73 remains published unchanged; 0.8.71 / 0.8.69 recovery retained. */
#define LF_VERSION "0.8.74-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.74"
/* Keep 0.8.73 touch wake, audio and screen-off timer unchanged.
 * Limit horizontal pager at physical ends; scroll animation cannot time out.
 * Host regression preflight passed. Physical screen wake/scroll test still required. */
