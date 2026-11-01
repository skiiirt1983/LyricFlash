# LyricFlash Alpha 0.3 — firmware source (not yet flash-ready)

Target: Waveshare ESP32-S3-Touch-AMOLED-1.75 (SKU 31261), 466×466, 16 MB flash, 8 MB PSRAM. Dedicated lyrics display: **no PrintSphere features**.

This revision extends Alpha 0.2 with:

- Read-only AXP2101 power telemetry using the **existing Waveshare BSP I2C bus**; green charging icon when USB/VBUS good, white on battery, amber <20%, red <10%. The charge bolt is used only for active charging.
- Self-contained Wi-Fi initial setup using a per-boot password-protected AP, and stored home Wi-Fi credentials in a stable NVS namespace.
- Local device page at port 80: Wi-Fi onboarding and manual, PIN-protected OTA upload, explicitly armed from the display for two minutes.
- Protection against 16 MB factory firmware being mistaken for a 5 MB OTA application.
- Existing animated demo lyric scroll, touch settings, color palette, size, brightness, NVS persistence and screen blanking.
- No codec, speaker, microphone, or remote recognition service is activated by this alpha.

## Files

`main/`: ESP-IDF firmware application. `partitions.csv`: matches PrintSphere 1.6.2, preserves 1 MB NVS and two 5 MB OTA app slots. `tests/`: host C/Python logic checks. `.github/workflows/firmware.yml`: reproducible cloud cross-build workflow. `BUILD.md`: how to build and safety gates.

**Important:** We cannot compile this ESP32-S3 target in the current authoring container, which lacks Espressif tools and network access to download managed components. A successful host test does not mean it boots. **Do not flash this source or an unvalidated binary.** A future alpha will implement audio recording and authorized song recognition after we verify hardware basics.
