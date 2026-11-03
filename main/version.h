#pragma once
/* 0.8.32: correct ES7210 24kHz 16-bit four-slot TDM microphone framing
 * using Waveshare's verified board voice profile (MIC1 slot0, MIC2 slot2,
 * MIC3 reference slot1, slot3 unconnected). Enable physical front-mic
 * input gain 24 dB, preserve explicit three-second Start/Stop and
 * report MIC1/MIC2 levels separately. Physical 0.8.31 had very weak
 * audio: quiet mean 0/peak26, loud mean38/peak243 /32768.
 * Preserve hardware-proven LVGL/QSPI display, OTA and NVS unchanged.
 * Requires new physical ADC gain/framing validation before recognition. */
#define LF_VERSION "0.8.32-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.32"
