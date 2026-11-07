# LyricFlash project credits and third-party notices

**LyricFlash application creator and maintainer: skiiirt1983.**

The following third-party components retain their original creators, copyright notices, and licenses. LyricFlash project authorship does not claim ownership of these upstream works.

## Hardware display component attribution

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

## Clock typeface assets (draft 0.8.62)

LyricFlash uses glyph-subset conversion at build time for the following genuine
Google Fonts families: Kablammo, Mystery Quest, Moirai One, Rampart One,
Hanalei, Bitcount Grid Double, Iceberg, Montserrat, Sancreek, Modak,
Alumni Sans Collegiate One, Honk, and Nabla.

Original upstream source files and licensing notices are hosted by Google at
https://github.com/google/fonts/tree/main/ofl . Each font's upstream OFL.txt
is available in its corresponding family directory; all generated glyphs are
redistributed under the applicable SIL Open Font License 1.1 terms. See
`tools/generate_clock_fonts.py` and the generated per-family manifest for
precise source paths and SHA-256 checksums.

Honk and Nabla are multicolour variable typefaces; the embedded display
currently uses single-colour antialiased glyph conversion, not their full
COLRv1 layered effects. The additional open-source Carlito, Tinos and Asap families are included
as substitutes for Calibri, Times New Roman and Trebuchet MS respectively.
These are not the proprietary Microsoft originals.

Only clock glyphs, not complete font files, are
included in the application firmware to fit both OTA partitions.

Proprietary Calibri, Times New Roman, and Trebuchet MS font files have NOT
been embedded or redistributed. The web preview may use browser-installed
system fonts and fallback fonts, but firmware only uses licensed font assets.
