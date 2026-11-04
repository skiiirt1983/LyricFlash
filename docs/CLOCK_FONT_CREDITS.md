# Clock glyph licensing and attribution

LyricFlash renders digits by rasterizing original open-licensed typefaces into
small 15 × 21 monochrome bitmap tables. **No TTF or OTF font files are
distributed**. The source of the glyph data is in `main/basic_open_face.h`.

Existing source digit families:
- **EB Garamond** — SIL Open Font License 1.1, original by Georg Duffner.
- **Noto Serif Display** — SIL Open Font License 1.1, Noto project.
- **Comfortaa** — SIL Open Font License 1.1, Johan Aakerlund.

Additional digit families (v0.8.61):
- **Cabin Bold** — SIL Open Font License 1.1, Pablo Impallari.
- **Comic Neue Bold** — SIL Open Font License 1.1, Craig Rozynski.
- **Noto Serif Display ExtraCondensed ExtraBold Italic** — SIL Open Font License 1.1, Noto project.
- **Go Mono** — BSD 3-Clause license, Bigelow & Holmes / Go Authors.

The per-face Web UI retrieves these exact glyph bitmaps from the device,
making the browser and AMOLED previews consistent. Trademark and font names
belong to their respective projects. Further redistributions should retain
these attributions and license notices.
