# LyricFlash Alpha 0.3 – validation record

Date: 2026-11-01
Target hardware: Waveshare ESP32-S3 Touch AMOLED 1.75, SKU 31261
Firmware stage: **SOURCE DEVELOPMENT / NO VERIFIED BIN**

## Automated tests executed in the authoring environment

- Partition table equals the six entries parsed from the user's original PrintSphere 16 MB flash layout (actual available full-image dump).
- NVS persistent settings default, save/load, mock reboot and schema validation.
- Original sample LRC timing, repeated timestamps, progress and edge-case tests.
- ESP32-S3 application-image preflight checks; reject wrong chip, project and factory-only full images.
- Recognition service error categorization, including exhausted credits and rate limits.
- AXP2101 decoded USB/charging/battery states and colour rules.
- Static inspection for the PIN-protected, device-armed OTA upload path, required esp_ota_* calls, and no NVS partition erasure.
- Pi portal-compatible `/api/device` firmware identity path present.

## Not tested (DO NOT flash until resolved)

- Actual ESP-IDF 5.5.x build: unavailable toolchain and offline container cannot download BSP dependencies.
- Hardware display/touch, PMIC I2C bus arbitration, USB power state, touch wake or Wi-Fi AP operation.
- PrintSphere v1.6.2 cross-project OTA migration acceptance: unknown; do not bypass updater checks.
- ESP32 app image header/size checksum for this revision: **no binary exists**.
- Local OTA flashing to a real ESP32 and rollback, battery-safe update, any microphone/lyrics services.

## Risks and next release gates

1. Successful ESP-IDF cross compilation and inspection of the resulting `.bin` (via supplied GitHub Actions workflow or supported local IDF toolchain).
2. Resolve any BSP / LVGL API compile errors.
3. Audit first cross-project OTA validation rules against PrintSphere, use only compatible application-only images; otherwise use USB browser flasher.
4. Ensure the Waveshare factory USB restore remains available.
5. Make first on-device test with USB available; confirm touch/display/PMIC, Wi-Fi, OTA server, setting survival.
6. Only then implement real manual mic recording, AudD, authorized lyrics display, and higher-level settings for beta 0.x.

No claim of a bootable firmware artifact is made by this package.
