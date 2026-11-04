#pragma once
/* Private alpha 0.8.57: persistent per-track lyrics/timing cache, original icon and navigation recovery
 * lyric display, independent Weather Studio, four-bar Web UI battery,
 * customizable Basic clock layout and original numeral styles.
 * Host regression tests and strict C parser checks passed; hardware test pending. */
#define LF_VERSION "0.8.57-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.57"

/* Release gate: lyrics progress without status label; enclosed mic gain 27 dB. */
/* Preflight validated; run the single ESP32-S3 release build for the
   measured audio waveform and bounded AudD response recovery. */
