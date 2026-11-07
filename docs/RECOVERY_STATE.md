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
- Firmware version labels alone do **not** confirm which source commit is currently running on the device. The physical installation must be checked in LyricFlash Web UI / Firmware after flashing.
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

## Hardware-tested baseline and first isolated backend change
- User confirmed 0.8.67 all features functional, and finger-release black flash fixed.
- **Pinned safe branch:** `safe/0.8.67-hardware-tested` points to exactly `498b0400e3965b4e7b6a75d7ff1151e2cc0f210c`, the built, immutable OTA image. This is the known-good rollback target; do not rewrite.
- Next branch: `feature/private-0.8.68-ota-health-gate`. **Only OTA startup confirmation** is changed; no audio recognition, synced lyrics, clock, touch, settings schema or battery modifications.
- Prior behavior: `app_main` accepted the OTA slot immediately after display and local Web server startup, even if the LVGL worker then stopped updating.
- Candidate: on a pending OTA slot only, an independent verification task requires 3 consecutive samples (2.5s apart) of responsive LVGL heartbeat, completed frames and at least 16KiB free heap. It will roll back on failure after about 40s. No Wi-Fi/cloud connection check (fallback setup AP is valid). Ordinary boots create no verifier task.
- Test the pure gate logic on host with boundary cases. Do not ship changes to battery management until this update is verified on hardware.

## Working 0.8.68 checkpoint and isolated 0.8.69 waveform gain
- 0.8.68 published as `alpha-build-fc303f347d32` with full CI/OTA/recovery validation. User reported all features working. Pin as `safe/0.8.68-working-checkpoint` at source SHA `fc303f347d32f5b7620cb468b572a38b945a5b07`. Do not rewrite the working 0.8.67 fallback either.
- Next isolated branch `feature/private-0.8.69-waveform-amplitude`: user's hand-drawn line was only a HEIGHT reference, not a request to change the real microphone waveform shape, color, line thickness or motion.
- Increase only the visual vertical amplitude: `swing=magnitude*8` instead of `(magnitude*7)/2`, or ≈2.29x for nonclipped excursions. Existing polarity, signed MIC1/MIC2 input, visual bias correction, clamp at 108, 75ms update interval, horizontal positions, ring, waveform color, Listening caption, WAV/AudD pipeline and UI layout stay identical. Actual hardware height is pending approval.
- Battery optimization will follow in a distinct future version.

## 0.8.71 isolated touch-wake lifecycle repair (hardware verification pending)
- User observed that after extended screen-off, holding finger revealed the UI but releasing could black it out. This was reported around the 0.8.70 battery project; **installed firmware version was not verified**, so do not assume 0.8.70 caused it or that deep sleep ran.
- Preserve working 0.8.69 release `alpha-build-8931200e16f7` exactly; experimental 0.8.70 release is NOT the basis for this fix and should not be recommended until wake reliability is verified.
- New `feature/private-0.8.71-wake-release-reliability` branch starts from 0.8.69. Wake event now restores brightness and makes opaque black cover transparent **without destroying the pressed LVGL target**. The original cover still captures the first physical touch and prevents tap-through. When the pointer reports RELEASED, `on_tick` queues one async cleanup. Cleanup checks that the physical pointer is still up before deleting the cover and invalidates the existing screen. This removes a credible finger-release race but requires on-device proof.
- No separate battery saving here: 0.8.71 intentionally retains 0.8.69's 16ms timer. No deep sleep, NVS, Wi-Fi, power management, microphone, recognition, waveform, lyrics or clock UI changes.
- Hardware test: leave screen off >5 minutes, press/hold, release, verify the screen stays lit through repeated clock/home/settings cycles, and that the wake tap did not click controls. Inspect wake counters if failure persists. Continue battery optimization only after passing this test.

## 0.8.71 to 0.8.72: re-apply the battery timer optimization without regressing wake
- `0.8.71-alpha` compiled/published as immutable `alpha-build-e5a09f92e9ac`, source SHA `e5a09f92e9acf5610a89f3c3b2f51befa83d4d87`. User reported positive initial screen-off/wake behavior on the Device Settings page (not an explicit confirmation that both 5- and 10-minute tests passed). Pin source at `safe/0.8.71-device-wake-tested` and retain 0.8.69 fallback.
- `0.8.72` source is branched **from 0.8.71**, not 0.8.70. Port only the successful 0.8.70 16 ms clock-sweep timer pause during black screen-off cover. Resume after finger RELEASE, async cover deletion and screen invalidation. Retain unchanged 0.8.71 no-tap-through lifecycle, 50ms touch polling, manual recognition, lyrics, waveform amplitude, Wi-Fi/OTA, brightness and settings.
- No deep sleep: default remains disabled; never promote untested GPIO11 sleep/wake behavior. No claimed runtime/mAh saving without hardware measurement.
- Preflight-only until wake tests and battery-stage build are separately validated; no 0.8.72 release should supplant the 0.8.71 recovery.

## 0.8.73 proposed touchscreen/Device Settings blackout safety fix (hardware pending)
- User observed **0.8.72 failed** the real 5-minute screen-off/wake test: press lights UI, release blacks it out repeatedly until a swipe; downward swipes while Device Settings is active produce intermittent black screen, Home only rarely immediately after a settings glitch. The long battery standby test was postponed; these observations supersede the initial positive 0.8.71 short test.
- Code audit found a real source-level bug: nested LVGL Device Settings viewport/brightness slider do not always bubble their PRESSED events, and the on_tick physical-touch fallback neglected `activity()`; 15/30/60s screen-timeout could occur while touching/dragging because sleep ignored finger-down state.
- New `feature/private-0.8.73-device-settings-wake-guard` starts from compiled 0.8.72 without altering 0.8.72 itself. The 50ms on_tick now refreshes `last_activity_ms` whenever the physical pointer is down (even during nested scroll and any nav early return), and a held finger explicitly blocks screen-off. `sleep_display` also guards against an active finger.
- On wake, opaque cover is switched to transparent for **both default and pressed LVGL states**, preventing theme pressed-state opacity from reverting black during release. Prior 0.8.71 deferred overlay deletion (after physical release), and 0.8.72 clock-sweep pause/resume, remain intact.
- Unproven until hardware tests whether this also resolves every intermittent blackout (other suspects: QSPI partial-refresh / invalidation, captured overlay input, touch reporting). Do not claim a confirmed physical fix before repeated Device Settings vertical swipes, five-/ten-minute screen-off wake/release and touch/no-tap-through tests.
- No deep sleep, Wi-Fi mode, mic/recognition, lyrics, waveform gain or OTA alterations. Keep 0.8.71 checkpoint and 0.8.69 as earlier recovery references.

## 0.8.74 right-edge pager guard + postwake swipe timeout
- Latest device observation Nov 4: sometimes a narrow **white slice beyond the rightmost Settings pager page** while swiping from physical far-right edge. After a 5min screen-off, woke, then two right-to-left swipes caused a black screen; touch woke it again, event stopped repeating. User did not identify installed version in that last report. **Do not claim root cause proven or any release hardware-accepted.**
- `feature/private-0.8.74-pager-edge-wake-activity` branches from published 0.8.73 with no battery hardware, deep-sleep, recognition, waveform or OTA source changes.
- Once a primary pager page is committed, restrict allowed scroll directions: Clock leftmost = toward Home only, Settings rightmost = toward Home only, Home = horizontal both directions. Disable horizontal scroll chaining out of this pager; elastic and momentum were already disabled. The goal is to prevent rendering an outside fourth page during a hard edge swipe, while preserving Home↔Clock/Settings navigation.
- On scroll BEGIN / SCROLL / END, refresh user activity so active page flips are never mistaken for idle. Prevent auto-off during active pager scrolling/snap and navigation reveal, including the defensive `sleep_display` call.
- Source regression `tests/test_pager_edge_timeout_contract.py` required; hardware check should distinguish **actual screen-off** (wake counter/last screen-off duration) from **unpainted black area** (OLED brightness active but UI blank) using LyricFlash Web UI diagnostics.
- The 0.8.71/0.8.69 checkpoint sources and 0.8.72/0.8.73 prereleases remain unchanged. Deep sleep remains off.

## 0.8.75 correct LVGL edge-direction mapping; do not flash 0.8.74
- Before recommending 0.8.74, upstream LVGL 9 source was independently audited. In `lv_indev_scroll.c`, positive pointer X movement is allowed by `LV_DIR_LEFT`, negative by `LV_DIR_RIGHT` (`diff_x` moves the pager's child pixels, not the viewport). The initial 0.8.74 mapping incorrectly treated these names as physical finger directions and may block the desired **Settings→Home and Clock→Home** swipes.
- A second, corrected version **0.8.75** changes only the mapping: Clock, the leftmost page, allows a leftward physical finger drag toward Home (`LV_DIR_RIGHT`); rightmost Settings allows a rightward finger drag toward Home (`LV_DIR_LEFT`); Home permits both directions. Keep horizontal chain disabled. Test contract now requires correct mapping and explicitly forbids reversed mapping.
- 0.8.74's native preflight passed but was insufficient to detect the semantic mistake. It must be treated as **superseded/untested, do not install** even if automatically published as an alpha prerelease. Release 0.8.75 after passing an independent preflight and compilation. All earlier checkpoints remain intact.
- Neither version proves the occasional post-wake black-screen is fixed without hardware testing. Preserve deep sleep OFF and original 0.8.73 wake guard + audio/OTA behavior.

## 0.8.76 return-swipe regression fix (hardware test pending)
- User reports they **cannot swipe back to Home from Settings** after the 0.8.74/0.8.75 edge-clamp work. Which specific build is running was not explicitly reconfirmed, but 0.8.75 was published. Treat as genuine broken navigation; preflight contract assertions did not prove physical UI behavior. **Do not recommend 0.8.74 or 0.8.75.**
- New branch `feature/private-0.8.76-restore-settings-home-swipe` based on published `0.8.75`. Removes dynamic `lv_obj_set_scroll_dir(...)` from `primary_pager_commit`. Pager now stays `LV_DIR_HOR` as in the previously navigable 0.8.73 release; no enforced single-direction on Settings or Clock. This gives both physical directions to LVGL; Settings must support swipe-to-Home, even if the unused outward gesture causes a narrow repaint anomaly (to be investigated separately rather than break navigation).
- Retain 0.8.73 nested Device Settings activity and wake pressed-state guards, 0.8.74/0.8.75 no-elastic and no-horizontal-chain flags, and active swipe/snap timeout exclusion. No changes to deep sleep, recognition, lyrics, battery optimization, or OTA.
- Revised `tests/test_pager_edge_timeout_contract.py` **forbids** changes to allowed pager scroll direction during page commit; explicitly checks unconditionally bidirectional `LV_DIR_HOR` at creation and the lack of elastic overscroll/chaining. This proves source structure, **not** functional on-device swiping.
- Acceptance: return Settings→Home by left-to-right swipe, Home→Settings by right-to-left, return Clock→Home by right-to-left; test 5min screen-off/wake and repeated outward Settings swipes separately. If white slice or unexpected blackout persists, gather `ui_wake_count`, `ui_page`, and render/flush counters to distinguish timeout vs invalidation. Preserve 0.8.69/0.8.71 recovery and all release artifacts.
