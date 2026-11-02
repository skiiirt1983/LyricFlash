# LyricFlash Alpha 0.8 — diagnostics and simpler controls

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
Run the GitHub Actions workflow for Alpha 0.8 and download `LyricFlash-Alpha-0.8-FULL-USB-and-OTA-UNTESTED`. The full USB `lyricflash-full.bin` is flashed at offset `0x0` on this specific ESP32-S3 16 MB device (or through the deployed Alpha/Beta browser installer). The standalone `lyricflash-ota.bin` is for a subsequent OTA update, never flash it at `0x0`.

Full merged USB firmware overwrites saved NVS and OTA selection data; note Wi-Fi settings before flashing. A build passing CI is not proof of hardware OTA compatibility.
