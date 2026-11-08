# LyricFlash

**Creator and maintainer:** [skiiirt1983](https://github.com/skiiirt1983)

LyricFlash is firmware for the Waveshare ESP32-S3 Touch AMOLED 1.75 (466×466 round AMOLED, 16 MB flash). It identifies the song playing around you, shows its timed lyrics, and doubles as a customizable clock.

Third-party libraries, board support and fonts keep their own authors and licenses; see [third-party notices](THIRD_PARTY_NOTICES.md).

## Current release: 1.0.2

- **Song ID and lyrics:** listens through the built-in microphones, identifies the song and shows synced lyrics. Per-song timing and tempo adjustments are saved.
- **Smooth navigation:** swipe between Clock, Home and Settings at 25–60 fps, with drag-to-follow and a settle animation.
- **Clock:** one customizable face with 20 fonts, adjustable size and color, date, battery and live weather. Press and hold the clock for its settings.
- **Weather:** current conditions for a chosen city, refreshed on a set interval. Swipe up on the clock for details.
- **Battery:** the CPU drops to 80 MHz with the screen off, touch and screen polling slow down, and the Wi-Fi radio sleeps during long idle periods. Standby drain is about 1% per hour.
- **Web pages:** the device serves settings, clock customization, weather, diagnostics and firmware updates at `http://<device-ip>/`.
- **Reliability:** a display watchdog restarts the UI if it ever stalls. Diagnostics are at `http://<device-ip>/api/diagnostics`.

## Updating a device that already runs LyricFlash

Open `http://<device-ip>/`, go to **Firmware** and install the newest version. Saved Wi-Fi and settings are kept.

## New install over USB

Download `LyricFlash-<version>-USB-and-OTA.zip` from the [latest release](https://github.com/skiiirt1983/LyricFlash/releases/latest) and follow `USB-INSTALL.txt` inside it. The full USB image is flashed at offset `0x0` and erases saved settings. The `lyricflash-ota.bin` file is only for over-the-air updates; never flash it at `0x0`.

## Building and releasing

See [BUILD.md](BUILD.md).

## Version history

The full per-version change log is kept in [main/version.h](main/version.h).
