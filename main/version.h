#pragma once
/* Alpha 0.8.36: collect microphone near-rail burst patterns WITHOUT changing
 * the working Waveshare 24kHz four-slot TDM mode or 24dB input gain.
 * Record contiguous near-rail burst count/max duration for MIC1 and MIC2,
 * simultaneous rail frames, half-scale sample counts, and RMS excluding
 * near-rail values. Preserve manual three-second capture and privacy:
 * no raw audio saved, uploaded, or streamed. These metrics diagnose but
 * do NOT yet classify the cause of sporadic full-scale samples.
 * QSPI/LVGL clock/touch, OTA three-row Current reinstall, and NVS unchanged.
 * Physical audio check required before capture can be considered clean. */
#define LF_VERSION "0.8.36-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.36"
