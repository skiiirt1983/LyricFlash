#pragma once
/* 0.8.33: keep hardware-confirmed two-mic ES7210 24kHz four-slot TDM
 * 0.8.32 audio input configuration and 24dB gain EXACTLY unchanged.
 * Add read-only 16-bit per-channel integer RMS and near-rail sample counts
 * (abs(sample) >=32760) to distinguish persistent clipping from occasional
 * large artifacts. Zero audio is recorded or uploaded. Existing Start/Stop,
 * QSPI/LVGL screen, OTA, Wi-Fi, and NVS are unchanged.
 * On actual 0.8.32: quiet MIC1/2 means=6/4, peaks=982/1041;
 * music means=118/75, peaks=32767/32768;
 * speech means=127/131, peaks=32767/32767.
 * Hardware assessment required before adjusting microphone gain/recognition. */
#define LF_VERSION "0.8.33-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.33"
