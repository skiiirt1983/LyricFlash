#pragma once
/* LyricFlash 0.8.59 private alpha: UI cancellation recovery, based on
 * hardware-verified 0.8.57 f991153d8da6 (2/2 identified songs).
 * AudD, microphone, LRCLIB, recognition and local cache sources are pinned.
 * Original 0.8.57 remains available at safe/0.8.57-hardware-verified. */
#define LF_VERSION "0.8.59-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.59"

/* 0.8.58 visuals retained: transparent speed overlay, auto-save, RESET,
 * actual microphone waveform, user logo and compact Home.
 * 0.8.59: cancellation handled after touch release on LVGL timer; explicitly
 * unlock pager, stop redundant hidden-overlay redraws; center waveform via
 * display-only slow-bias removal. Hardware testing required before safe tag. */
/* Source preflight passed; release-gated ESP32-S3 firmware validation. */
