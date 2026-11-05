# LyricFlash

**Creator and maintainer:** [skiiirt1983](https://github.com/skiiirt1983)

This repository contains the LyricFlash application. Third-party libraries, board support and fonts retain their own authors and licenses; see [third-party notices](THIRD_PARTY_NOTICES.md).

## Historical Alpha 0.8.1 notes

Development firmware for Waveshare ESP32-S3 Touch AMOLED 1.75 (466×466, 16 MB).
Public stable v1.0 is not released; do not promote this build without hardware validation.

## Improvements over Alpha 0.7
- Full-screen slide animations replaced by immediate screen swaps; button gradients/shadows reduced to improve responsiveness.
- Swipe DOWN from Home to open Device Diagnostics, then UP or BACK HOME to return. Shows version, IP, connected SSID/RSSI, uptime, internal heap/PSRAM, max UI tick gap, battery/USB/charge, OTA state, microphone OFF, and sanitized application errors.
- Read-only network JSON diagnostics: `http://DEVICE-IP/api/diagnostics`.
- Local `http://DEVICE-IP/settings` Basic Clock Studio: font preset, time size, ring mode, background/foreground/ring colors, date, weather, weather city and refresh interval. Save requires a one-time PIN on the device via Device Settings → ARM WEB / OTA.
- Adds visible SPACE key to the character wheel for two-word weather locations.
- Preserves Alpha 0.7's corrected lyric preview scrolling, Rolex clock and OTA diagnostics. OTA is still awaiting physical update testing.

## Installing the full USB image
Run the GitHub Actions workflow for Alpha 0.8 and download `LyricFlash-Alpha-0.8.1-FULL-USB-and-OTA-UNTESTED`. The full USB `lyricflash-full.bin` is flashed at offset `0x0` on this specific ESP32-S3 16 MB device (or through the deployed Alpha/Beta browser installer). The standalone `lyricflash-ota.bin` is for a subsequent OTA update, never flash it at `0x0`.

Full merged USB firmware overwrites saved NVS and OTA selection data; note Wi-Fi settings before flashing. A build passing CI is not proof of hardware OTA compatibility.

## Alpha 0.8.1 hotfix — gesture reboot protection

A physical-device test found that Alpha 0.8 may reboot during a swipe.
Root-cause hypothesis: synchronous `lv_obj_delete(old_screen)` while an LVGL
gesture/click event is still being dispatched. This hotfix changes that to
`lv_obj_delete_async(old_screen)`, retaining instant screen swaps but deferring
old-screen destruction until LVGL has finished handling the input event.

The source test explicitly rejects reintroduction of synchronous deletion.
The patch is not hardware-verified until swiping in all directions on the actual
Waveshare ESP32-S3. Alpha 0.8 remains unmodified as a recoverable prior build;
use this OTA image on 0.8 if its local updater still works.
