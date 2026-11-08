# LyricFlash consolidated alpha validation

This is a private development build for Waveshare ESP32-S3 Touch AMOLED 1.75".
Do not merge into main, publish to public Pages, or claim hardware verification.

## Passed source checks

- Latest full host suite and embedded browser JavaScript tests: GitHub Actions workflow #111.
- All six clock SVG previews: dynamic colours, independent profiles, face switching, reset, and saved JSON verified with a simulated DOM.
- Settings swipe-back and lightweight transitions, USB charge colour, AP credentials, weather configuration refresh, and PIN-free saving are source-checked.
- Wi-Fi settings can be updated from normal home LAN Web UI.
- Firmware selection defaults to no build selected, and GitHub Pages catalog uses verified LyricFlash OTA app descriptors.

## Hardware acceptance test after successful compile

1. USB or app-only OTA install preserving home Wi-Fi (do not write a merged full image at 0x0 as an update).
2. Touch menu: Settings swipe LEFT and RIGHT returns Home, submenus swipe Back.
3. Swipes follow the finger and settle without overlapping screen trees, lag, or buttons catching gestures.
4. Clock hold menu at 18/20/24 pt: CANCEL and SETTINGS labels remain on one line.
5. Six clock faces, browser live preview, independent colours and per-face persistence after restart.
6. Weather details load after configuration save, then update on schedule, including offline recovery.
7. GitHub firmware page: dropdown lists actual verified published versions; update no PIN; NVS unchanged.
8. Lyric scroll cap: new 100% equals prior 35%; low speeds stay smooth.
9. Charging colours, diagnostics, touch wake, memory and battery behaviour.
10. Verify against physical Waveshare before any public v1.0.

## Scope caveats

- Genuine microphone recognition and synchronized lyrics are NOT implemented in this demo.
- Preview and firmware UI still require visual inspection on physical hardware/browser.
- Public installer must contain no PrintSphere references.
- This artifact is untested hardware firmware, not stable.
