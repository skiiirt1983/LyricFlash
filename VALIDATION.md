# LyricFlash 1.0.0 – validation record

Target hardware: Waveshare ESP32-S3 Touch AMOLED 1.75, 16 MB flash.

## On-device testing (owner, 0.8.125–0.8.130)

- Swiping between Clock, Home and Settings: 22–27 fps typical, no freezes, blackouts or start-up flashes.
- Song ID and lyrics working, including failed and cancelled searches.
- Clock face with weather, date, battery and AM/PM. Clock settings menu. Correct local time on every screen, including right after start-up and wake.
- Overnight standby: 9.5 hours of uptime with no restarts, display timeouts or errors. Battery drain about 1% per hour with the screen off.
- Over-the-air updates from the watch's Firmware page.

## Automated checks on every build

- Host-side tests (`tests/run_tests.sh`): settings persistence and migration, lyric timing, OTA URL and version handling, web-page scripts, and source contracts that guard against previously fixed display, touch and wake bugs.
- ESP-IDF compile, OTA image and full USB image validation, and checksummed release packaging before anything is published.
