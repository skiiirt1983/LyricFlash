# Building LyricFlash

## Releases (GitHub Actions)

The **LyricFlash <version> - ESP32-S3 Build** workflow (`.github/workflows/firmware.yml`) runs on every push to `main` that changes firmware sources, and can also be started manually from the Actions tab. It:

1. Runs all host-side tests (`bash tests/run_tests.sh`).
2. Generates the embedded clock fonts and compiles with ESP-IDF 5.5.4 for the ESP32-S3.
3. Validates the OTA image and the merged full USB image, and packages the USB-and-OTA ZIP.
4. Publishes the build for on-device updates, as an `alpha-build-<commit>` prerelease with a direct OTA image. Every LyricFlash updater, including 0.8.x, finds updates this way.
5. For a stable version (no `-alpha`, `-beta` or `-rc` suffix), also publishes a normal `v<version>` release marked Latest.

To release a new version:

1. Update `LF_VERSION` in `main/version.h`, `PROJECT_VER` in `CMakeLists.txt`, the version strings in `.github/workflows/firmware.yml`, and the version-pinned tests.
2. Add a change-log line to `main/version.h`.
3. Push to `main`.

Use 1.0.x for fixes and 1.x.0 for new features.

## Rules for all future versions

- **Setup network:** when the device has no working Wi-Fi it starts an open network named `LyricFlash-XXXX` (no password).
- **Captive portal:** joining that network must open the LyricFlash page by itself on phones and computers. Three pieces make this work, and tests enforce all three:
  - DNS answers every lookup with the device's address (`main/captive_dns.c`).
  - Unknown pages redirect to the home page, on the setup network only (`handle_not_found` in `main/network.c`).
  - The portal address is sent in DHCP option 114.

## Local build

With ESP-IDF 5.5.4 installed:

```bash
python3 tools/generate_clock_fonts.py
idf.py set-target esp32s3
idf.py build
python3 tools/apply_adapter_patch.py
idf.py build
python3 tools/check_build.py build/lyricflash.bin
```

Font generation needs `lv_font_conv` (`npm install --global lv_font_conv@1.5.3`). `build/lyricflash.bin` is the over-the-air image; `idf.py merge-bin -o lyricflash-full.bin -f raw` produces the full USB image.

## Tests

`bash tests/run_tests.sh` runs the host-side logic, contract and web-page tests (needs Python 3, Node.js and a C compiler). They do not replace a full ESP-IDF compile or testing on the device.
