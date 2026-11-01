# Getting a compiled LyricFlash Alpha 0.3 image

This is a reproducible ESP-IDF **build kit**, not a pre-compiled or hardware-tested release.

## Fastest option: build on GitHub (no ESP-IDF installation on the Pi or PC)

1. Create a **private** repository at https://github.com/new (e.g. `LyricFlash`).
2. Upload the entire contents of this folder, including the hidden `.github` directory. An easy way is to clone the empty repository using GitHub Desktop, copy the contents of this ZIP into the cloned directory, commit, and push. (Web upload may also work if it preserves `.github/workflows/firmware.yml`.)
3. In GitHub, go to **Actions -> LyricFlash Alpha 0.3 - ESP32-S3 Build -> Run workflow**.
4. If the build passes, open its run and download the `LyricFlash-Alpha-0.3-ESP32S3-UNTESTED` artifact ZIP. It contains the application-only `.bin`, `build-checks.json`, and SHA256 file.
5. Upload the artifact ZIP or the build log here for inspection **before flashing**. If the build fails, share the complete build error; it identifies source/API mismatches to correct.

This workflow uses the pinned Espressif `espressif/idf:v5.5.4` toolchain and explicitly pins LVGL 9.5.0. GitHub downloads managed Waveshare and LVGL dependencies during compilation.

## Safety and firmware limitations

- This build is **not yet a finished lyrics product**; the Alpha 0.3 source uses demo lyrics and does not record audio, identify songs or provide true sleep.
- The artifact is **application-only OTA**. Never flash it at 0x0 as a full/factory image.
- Compilation and preflight checks prove neither real-device operation nor that PrintSphere 1.6.2 accepts a different project in its OTA uploader.
- The existing PrintSphere firmware is still the working version; do not replace it until a recovery pathway and migration compatibility are verified.
- The source intentionally does not automatically erase NVS. Future LyricFlash OTA updates retain settings as long as the partition layout is unchanged.

## Alternative local build

On a machine with ESP-IDF v5.5.4 configured and internet access to managed components:

```bash
idf.py set-target esp32s3
idf.py build
python tools/check_build.py build/lyricflash.bin
```

The resulting file `build/lyricflash.bin` is an application-only image, **not yet hardware-validated**.
