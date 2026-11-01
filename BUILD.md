# Building LyricFlash Alpha 0.3

**Source under test, not yet verified as a flashable release.**

## Preferred: GitHub Actions

Create a new private GitHub repository and upload the *contents* of this folder (including `.github/workflows/firmware.yml`). Run **Actions → LyricFlash Alpha ESP32-S3 build → Run workflow**. If and only if the build succeeds, the workflow attaches `build/lyricflash.bin` as an artifact. This is an **application-only** image, not a factory restore image. The CI workflow enforces an ESP32-S3 header and 5 MB slot maximum.

You can also build locally with an ESP-IDF 5.5.x environment:

```
idf.py set-target esp32s3
idf.py build
```

The managed component manager downloads Waveshare's v3.0.1 board package and LVGL 9.5. Both require internet access.

**Not yet confirmed:** PrintSphere 1.6.2 cross-project OTA validation, physical display/battery/wake compatibility, and the initial OTA rollback behavior. Until those are verified, **do not upload** a generated image onto your current ESP32. The first build may expose source/API mismatches requiring changes.

## Built-in alpha features

- Original-song-text demo preview (no copyright material), colour/font size/brightness/scroll speed settings, with NVS persistence.
- AXP2101 read-only battery percentage, charger indication and charge-aware icon colour. No speaker, music scanning, or microphone recording occurs.
- If saved home Wi-Fi works, device serves `http://<displayed-IP>/` with local settings and PIN-protected OTA. If not, ESP32 exposes a WPA2-protected setup access point. Credentials survive future *application-only* OTA.
- To update: on ESP32 home tap **ARM OTA**, then in its web page enter the displayed 8-digit PIN and upload the **application-only** firmware. OTA is allowed for 2 minutes per arm.
- Firmware header checked before writing; ESP-IDF's final integrity check remains authoritative.

## Known limitations

- Sleep blacks out AMOLED and disables scrolling, but *does not* yet enter true deep sleep. Touch-to-wake requires hardware verification.
- Wi-Fi onboarding is a first-stage LAN-only configuration (not HTTPS). Do not expose port 80 or the device access point to the internet.
- No real song recognition, lyrics web API, microphone recording, battery fuel-gauge calibration, Wi-Fi credentials encryption, or lyric acquisition yet.
- A failed cross-project OTA from PrintSphere may require USB ROM recovery. OTA rollback depends on the bootloader already installed; compatibility of that bootloader's rollback settings is not guaranteed.
- Manufacturer factory restoration always uses the separate Pi-hosted recovery installer, not the application-only OTA endpoint.

## Tests

Run `bash tests/run_tests.sh` for the host-side logic tests (requires C compiler and Python 3). These tests **do not** replace a full ESP-IDF compile or on-device testing.

For a step-by-step cloud compilation guide, see README_BUILD_FIRST.md.
