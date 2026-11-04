# LyricFlash recovery checkpoint — 2026-11-04

## Source of truth
- Repository: `skiiirt1983/LyricFlash`.
- This checkpoint branch: `feature/private-0.8.66-recovery-polish`, based on `247bf6d9ebbc714050fa181f526118f0c73834ae`.
- Earliest safe recognition comparison point: `f991153d8da6f6f42bb204309d6cc074a5c17ad9` (0.8.57). In the 0.8.66 source, `main/recognition.c`, `main/recognition_audio.c`, `main/mic_pcm.c`, `main/mic_probe.c`, `main/lyric_sync.c`, `main/lyrics.c`, and `main/song_cache.c` have the same Git blob hashes as that revision. **Do not rewrite or replace them to adjust UI.**
- Last built and published 0.8.65 application source: `595a9caf14b873a5107f3fc17440424b4618f467`, release tag `alpha-build-595a9caf14b8`. Subsequent 0.8.65 fixes at `1bc4474...`, `b2cdb9e...` and `247bf6d...` passed source tests but were **not** compiled as firmware.
- `main` is intentionally still at older Alpha 0.8.3. Do not merge or rebase to `main` as a shortcut.

## Recovery changes staged for 0.8.66
1. All standard screen buttons use the Device Settings glass style from startup and on return; the dedicated Home Play button keeps its separate design.
2. Device brightness slider enlarged from 242 x 14 to 242 x 32 touch track; drag gestures do not bubble into parent navigation; settings persist on release.
3. Clock Studio full-spectrum controls show live colour swatches rather than a visible HEX string. Underlying HEX values remain for save/API.
4. Clock Studio hides the old redundant Basic Custom panel; the single **Reset to defaults** button resets the active face, and for Basic also resets its legacy position/size fields.
5. Firmware version, ESP-IDF project descriptor, private build artifact names, CI branch gate and release tests must agree at `0.8.66-alpha`.

## Release safety
- Keep all existing 0.8.57/0.8.62/0.8.64/0.8.65 tags, recovery ZIPs and per-commit `alpha-ota/releases/<tag>/` images intact.
- Source edits without `[compile-once]` run preflight only, never compile or publish firmware.
- After preflight succeeds, request **one** intentional `[compile-once]` push; inspect GitHub Actions for a successful ESP32-S3 build, validated full USB+OTA package and immutable prerelease before declaring it OTA-ready.
- Firmware version labels alone do **not** confirm which source commit is currently running on the device. The physical installation must be checked in Device Studio / Firmware after flashing.
- Never full-flash or erase the device for an ordinary OTA update; preserve Wi-Fi, clock settings and per-song history.

## Physical-device acceptance checks (pending)
1. Boot and navigate Home → Settings → Device → back; all ordinary buttons retain the same polished style.
2. Move the brightness slider at 20%, 50%, 80% using a finger; vertical scrolling must not steal the drag; value persists after return.
3. Open Clock Studio on a phone: all colour fields show their actual colours, picker edits update preview, and only one face reset is visible.
4. On Basic Custom, change time placement, date/battery placement, font and background, reset to default, then save and reload; verify no unrelated face or weather settings change.
5. Sleep with the screen off for at least five minutes, wake several times, inspect responsiveness; long-sleep lag is **not yet confirmed fixed**.
6. Identify two known songs and verify saved lyrics, per-song timing and return to Home; recognition code is intentionally unchanged.
7. Test OTA install and restart while preserving Wi-Fi, clock settings and song cache. Keep previous release available for rollback/reinstallation.

No automated build or source-test result substitutes for these device-specific checks.

## Verified recovery preflight
- Source/native preflight passed on `c32dfde9852e26e837f7600918e6f954f8ec0fcc` (GitHub Actions run 41857987720); the ESP32-S3 compile job was intentionally skipped at that stage.
- The subsequent explicit compile gate will publish only if preflight, ESP-IDF compilation, image inspection, complete recovery package and OTA download verification all succeed.

## Built and published recovery release
- **0.8.66-alpha is now built and published**, from exactly `149714c7e215bdefa3379dcbebb08f20ff5e53d2`.
- GitHub Actions [run 41858180364](https://github.com/skiiirt1983/LyricFlash/actions/runs/41858180364): source/native preflight, all 16 fonts, ESP32-S3 compilation, full USB merge, OTA validation, checksum/package verification, direct OTA download and GitHub prerelease publication **all succeeded**.
- Immutable prerelease: [alpha-build-149714c7e215](https://github.com/skiiirt1983/LyricFlash/releases/tag/alpha-build-149714c7e215). Contains application-only OTA `lyricflash-ota.bin` and `LyricFlash-0.8.66-alpha-USB-and-OTA.zip`.
- Verified direct OTA also exists at `alpha-ota/releases/alpha-build-149714c7e215/lyricflash-ota.bin`. Older SHA-specific assets remain untouched.
- **Physical device verification is outstanding**. A green GitHub build does not prove on-device appearance, wake behaviour or song identification.
- Recommended path from an existing LyricFlash installation: open **Firmware → Refresh from GitHub**, select **New 0.8.66-alpha**, install via the device's Wi-Fi OTA. It should preserve NVS settings; verify on hardware.
- The separately hosted GitHub Pages beta USB installer still advertises historic Alpha 0.8.41 because `main` has not been promoted. Do **not** use that old beta page to obtain 0.8.66. Use the GitHub prerelease package for recovery, not as an OTA upload.
- A future **documentation-only** commit to this file does not change the compiled source or release SHA.

## Touch-release blackout: investigation and source patch
- Report: the panel intermittently goes black for roughly a second when a finger lifts.
- Dedicated 0.8.67 branch (preflight only initially): `feature/private-0.8.67-touch-release-repaint`.
- Scoped candidate: ignore bubbled child scroll events, move pager snap initiation out of LV_EVENT_SCROLL_END and into the existing timer/watchdog, repaint the settled active pager page once.
- Keep 0.8.66 immutable as fallback. User hardware verification still required; this is a plausible UI race, not a confirmed root cause.

## Published 0.8.67 release
- Build SHA: `498b0400e3965b4e7b6a75d7ff1151e2cc0f210c`.
- CI: [GitHub Actions run 41861389591](https://github.com/skiiirt1983/LyricFlash/actions/runs/41861389591); preflight, ESP32-S3 compile, full USB package, firmware verification, SHA-specific direct HTTPS OTA and prerelease creation succeeded.
- Immutable alpha prerelease: [alpha-build-498b0400e396](https://github.com/skiiirt1983/LyricFlash/releases/tag/alpha-build-498b0400e396). Includes application-only `lyricflash-ota.bin` and complete recovery ZIP.
- The previous `0.8.66-alpha` release and direct OTA remain available and unchanged.
- **Hardware validation outstanding:** repeat Clock ↔ Home ↔ Settings finger releases and Device Settings entry/exit; check for any black flash. If present, capture gesture/page and diagnostics so the next patch targets the actual blanking source.
- This checkpoint-only documentation update must not trigger compilation.
