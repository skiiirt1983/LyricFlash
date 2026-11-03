# Hardware display component attribution

LyricFlash includes the Apache-2.0-licensed Waveshare ESP32-S3
Touch AMOLED 1.75-inch board support component under
`components/esp32_s3_touch_amoled_1_75/`.

The version adapted here was published with the Cpt_Kirk/PrintSphere
repository (https://github.com/cptkirki/PrintSphere), as inspected on
November 2, 2026. The original component's Apache 2.0 license is retained in
`components/esp32_s3_touch_amoled_1_75/LICENSE`.

The component includes QSPI transfer staging from PSRAM through internal
DMA-capable memory, panel TE synchronization, and rotation support.
No printer-monitoring application code or artwork is incorporated.

LyricFlash integration modifications:
- Reference the vendored component instead of managed Waveshare BSP 3.0.1.
- Select TE-synchronized display mode and pin the LVGL task to CPU 1.
- Restrict queued SPI transfers and reserve internal DMA memory.
- Keep the panel producing TE while the display is covered in sleep mode.
- Preserve the original Wi-Fi configuration, NVS, and OTA update code.

This attribution does not imply endorsement by the original project.
