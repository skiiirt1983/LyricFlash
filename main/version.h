#pragma once
/* Alpha 0.8.37: First explicit manual AudD song-recognition prototype.
 * Add an opt-in token in NVS (never returned or logged), Web UI Start/Stop,
 * bounded 10-second 24kHz 16-bit MIC1/MIC2 stereo WAV capture in PSRAM,
 * HTTPS-only AudD multipart identification, and status/title/artist response.
 * Recording is never stored in flash or exposed on a local HTTP endpoint;
 * capture and external transmission require each manual scan confirmation.
 * This is deliberately browser-first. Device-screen song ID and synced lyrics
 * will be integrated only after the recognition pipeline is field-verified.
 * MIC 24kHz/4slot/24dB, LVGL/QSPI, touch, battery, OTA/NVS retained.
 * Upload uses AudD quota; no automatic background recording or retry.
 * Physical external service validation required. */
#define LF_VERSION "0.8.37-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.37"
