# LyricFlash Alpha 0.7 — full USB recovery and OTA update repair

Hardware: Waveshare ESP32-S3 Touch AMOLED 1.75 (466 × 466, 16 MB flash).
Private alpha/beta development only. Stable v1.0 is not yet released.

## What's in this revision

- Carries forward the screen transition, lyric preview scroll, clock, and weather improvements from unreleased 0.6.
- Local OTA form now reports actual file transfer progress, HTTP status, network errors and timeouts; it does not confuse upload completion with verified installation.
- OTA handler validates the image header and inactive partition, uses known-size erase rather than erasing the entire 5 MB slot, logs failures, and reports last OTA result and byte counts from /api/info.
- GitHub Actions packs both full USB image (lyricflash-full.bin) and application-only OTA image (lyricflash-ota.bin), plus SHA256SUMS.txt and beta manifest.

## Installing Alpha 0.7 by USB

Run the GitHub Actions workflow 'LyricFlash USB Build and Pages' from this Alpha 0.7 branch, or use its successful PR build. Download the artifact 'LyricFlash-Alpha-0.7-FULL-USB-and-OTA-UNTESTED' and extract it.

Use lyricflash-full.bin at address 0x0 with a compatible ESP32-S3 browser/serial flasher for a 16 MB flash device. Never flash lyricflash-ota.bin at 0x0. The full merged image contains FF padding over NVS and otadata, so flashing it will erase saved Wi-Fi/settings. Record anything needed before proceeding; no additional full-chip erase is required.

The project beta browser installer uses the verified full merged image after the branch is merged to main and GitHub Pages deploys; do not assume the live installer has Alpha 0.7 until it says so.

After USB boot, verify the touch interface, transitions, lyric scrolling, Wi-Fi, and API version. Then test an OTA to a later image using the on-device PIN. A passing build does not prove OTA works on hardware.

This is not an official/public stable firmware release and does not include PrintSphere functionality.
