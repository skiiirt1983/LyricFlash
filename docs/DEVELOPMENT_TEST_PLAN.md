# Physical-device regression and recovery — November 2, 2026

**0.8.29 hardware result: Mic ADC electrical detection PASS; display/touch FAIL.**

- ADC ES7210 responded at I2C address `0x40`; `microphone_adc_status=adc_detected`, attempts 1, error code 0. Audio capture is NOT yet tested.
- AMOLED was mostly white with no touch response; `display_render_count=1`, `display_flush_starts=2`, `display_flush_finishes=1`, `display_refresh_ready=0`, `ui_touch_presses=0`.
- The 96-row PSRAM LVGL rendering-buffer experiment inherited from 0.8.28 coincided with the first-refresh stall. It is now rejected as hardware-incompatible pending deeper investigation.
- **0.8.30 reuses the exact 0.8.26 display-driver blob**, retains the functioning 0.8.29 microphone electrical probe, and has no other functional changes.
- Stage 2 (PCM audio capture) is BLOCKED until 0.8.30 display/touch/first-refresh recovery is physically verified.
- Emergency immediate recovery: use the still-responsive local `/firmware` Web UI to restore 0.8.26-alpha over OTA without erasing flash or settings.

---
# LyricFlash — functional-device test sequence and deferred-change register

Baseline: `0.8.26-alpha` is hardware-proven for display/touch, while `0.8.28` and `0.8.29` failed screen/touch on physical hardware. `0.8.30` is the targeted display recovery candidate and is not hardware-proven until the user verifies it. Keep 0.8.26 as rollback. Do not merge or publish a stable v1.0 release until the device tests pass. Each functional milestone uses **one isolated change and one gated compile after preflight**, followed by a specific device acceptance test.

## Current milestone: Alpha 0.8.34 — 3-option OTA Web UI selector

- **User priority:** replace long GitHub firmware release list with a maximum of three items relative to the version installed on-device: **Old** (closest strictly older published version, disabled), **Current** (device-reported installed version, disabled) and **New** (newest strictly newer validated published OTA, only actionable option). If one category is absent, omit it. Never offer a fourth placeholder entry.
- **Failure safety:** if installed version cannot be read or parsed, display Current/Unknown only and disable OTA installation. Unknown/prerelease version formats fail closed. Deduplicate identical versions across alpha prereleases, beta catalog and manifest while preserving verified direct GitHub asset preference. Preserve HTTPS remote OTA, local OTA upload, update progress and errors.
- **Tests:** host-side firmware-browser runtime tests cover Old/Current only, all 3 categories, newest installed, no older available, malformed installed version, duplicate releases, empty/unreachable catalog, refresh, explicit New click, no downgrade, and preferring a verified direct GitHub OTA image. Physical firmware-page acceptance is required after full build.
- **No audio or display changes:** 0.8.33 dual MIC1/MIC2 24kHz four-slot TDM/24dB gain, per-mic RMS and near-rail count; QSPI/LVGL verified display; settings storage remain unchanged. Rare extreme PCM samples still need a future targeted test. Do not mix additional microphone or UI changes into this build.

## Previous milestone: Alpha 0.8.33 — measure RMS and near-rail artifacts

- **0.8.32 physical test: PASSED dual-microphone response.** Quiet room: 141,824 selected mic samples, combined mean 5, peak 1041; MIC1 mean/peak 6/982; MIC2 4/1041. Low music: 141,824 selected samples, combined mean 96, peak 32768; MIC1 118/32767; MIC2 75/32768. Loud voice: 142,336 samples, combined mean 129, peak 32767; MIC1 127/32767; MIC2 131/32767.
- **Problem:** rare max-amplitude samples may mean clipping or TDM transport glitches. A single peak cannot establish the prevalence or audio fidelity.
- **0.8.33: diagnostic-only**, no gain or audio mode changes. Each microphone now reports integer RMS (0–32768) and a count of near-rail values (abs(sample) >= 32760), alongside the existing mean/peak and sample count; percentage in Web UI is count divided by per-mic sample frames. Ignore reference TDM slot1 and unused slot3, and prevent integer overflow by 64-bit sum of squares. Counts and accumulated RMS are reset for each Start. No raw audio is retained.
- **Preflight passed:** native mock tests confirmed RMS, near-rail counts, silence, per-mic separation, reference/unused TDM slot exclusion, repeat reset and manual Start/Stop; static source/browser/release checks passed. Firmware build is gated to one `[compile-once]` source-identical commit.
- **Hardware acceptance:** physically run one quiet, one low-volume music and one speaking sample. Compare each MIC mean/RMS/peak/near-rail count; a few isolated near-rail events indicate transients/artifacts worth investigation, sustained near-rail events indicate clipping risk. Do not infer clean recognition input until this has been checked.
- **Safety gate:** preserve 0.8.32 24k four-slot TDM, physical MIC1/MIC2 mapping, 24dB input gain, manual-only 3-second Start/Stop, proven 24-row QSPI/LVGL display, OTA and saved preferences.
- **Top deferred change unchanged:** OTA Web UI picker Old / Current / New only, disabled older/installed entries and no unsupported downgrade.

## Previous milestone: Alpha 0.8.32 — ES7210 TDM receive correction

- **Physical 0.8.31 measurements (outside case):** quiet: 65,536 samples, mean 0, peak 26; low music: 65,536, mean 1, peak 40; loud voice: 65,536, mean 38, peak 243. Strong response to voice proves nonzero microphone path, but magnitude was too low for recognition. Stop early worked in 651 ms. No need for further duplicate audio sampling on 0.8.31.
- **Confirmed source/config mismatch:** original BSP uses 22,050 Hz standard mono I2S for the ES7210. Waveshare maintained Brookesia firmware instead captures at 24,000 Hz, 16-bit, 4-slot TDM; slot order MIC1, MIC3 playback reference, MIC2, MIC4 unused; gain applied to physical MIC1/MIC2 enable bits.
- **0.8.32:** use Waveshare's TDM RX plus standard stereo TX audio initialization, select three physically connected ES7210 inputs, configure 24 dB front mic gain, parse only slots 0 and 2, and report per-MIC mean/peak. One manual three-second capture at a time, Stop, no raw audio saved/uploaded. Preserve **exact hardware-verified 0.8.26/0.8.30 QSPI/LVGL values** and the existing on-device 0.8.31 UI/OTA behavior.
- **Preflight:** all static, native mocked-codec, browser and release-contract tests passed without compiling firmware. Reuse the ES7210 codec handle between recordings to avoid repeated allocations.
- **Physical verification required before proceeding:** install alpha 0.8.32 after CI passes, confirm screen and touch still work, then capture (a) silence, (b) music, (c) loud voice 5 cm from MIC1 and MIC2. Compare overall and per-mic levels; do not claim meaningful PCM quality based on samples alone. Stop and re-test if the device freezes, audio reports codec/gain error or output stays at 0.
- **Deferred top priority unchanged:** OTA Web UI picker maximum three entries Old/Current/New, with downgrade disabled. This audio-only change does NOT alter OTA UI.

## Previous milestone: Alpha 0.8.31 manual PCM sound-level test

- **0.8.30 hardware: DISPLAY/TOUCH PASS**, and ES7210 ADC I2C 0x40 ACK PASS. This is the physical stability baseline. Do not change the display-driver file.
- 0.8.31 adds one Start/Stop audio test to /settings; its ESP codec device is only opened after Start, reads mono 22,050 Hz 16-bit PCM for at most 3 seconds, then closes.
- No audio files, raw samples, audio HTTP data or cloud requests are retained or sent; a small transient PCM RAM buffer is processed for aggregate sound levels. The Start POST requires a custom header from the local Web UI, reducing cross-site activation risk. Only aggregate peak/mean/sample count and error state are returned to browser.
- **Hardware pending:** on 0.8.31, verify normal screen, Clock swipe, Wi-Fi, battery, OTA page. Run once in quiet room and once while speaking/clapping near microphones; compare `mean_abs`/`peak_abs`. Confirm Stop and repeat are responsive. Report `microphone_pcm_status` and `microphone_pcm_error` if failed.
- If samples are zero, values fixed, or the codec cannot open, do not interpret as passed. The bundled BSP's legacy standard-I2S mono path may need to be upgraded to the board-verified 24 kHz four-slot TDM path in a **separate** milestone; hardware reference serializes MIC1, MIC3/reference, MIC2, MIC4.
- **Known limitation:** the bundled BSP allocates I2S resources permanently after first initialization; `esp_codec_dev_close` stops active ADC recording but does not fully deinitialize the I2S peripheral. Full I2S teardown, power measurements and speaker coexistence tests are deferred. Do not claim deep-sleep power validated.
- The user-requested top-priority OTA dropdown (Old/Current/New, max 3, no unsupported downgrades) stays in the **deferred register**, not in this microphone firmware.

## One item at a time — execution order

| Order | Stage | Status | Hardware pass criterion |
|---|---|---|---|
| 1 | Manual ES7210 audio-ADC I2C ACK probe via Web UI | **PASS: ES7210 electrical probe on 0.8.29 hardware; remaining display/touch gate pending 0.8.30** | Device Web UI reports `adc_detected` from address 0x40; battery, Wi-Fi, touch and clock still operate |
| 2 | On-demand ES7210 + I2S PCM amplitude test | **0.8.33 signal response and RMS diagnostics physically tested; extreme-sample source under investigation** | User explicitly presses Start; live audio levels differ in quiet/noisy conditions; Stop reliably releases all capture resources; no audio uploaded |
| 3 | On-device lyrics view using a short original/local timed test fixture | Not started | Readable lines display in order and follow timestamps on the physical 466×466 screen without blocking touch/clock |
| 4 | Manual song-identification capture and recognition client | Not started | Start captures bounded sample, communicates with configured service over TLS and displays track or clear no-match/quota/error state |
| 5 | Fetch, parse and cache synced lyrics | Not started | Matched track loads legally available lyric text, timestamps and source/error state; limit external requests |
| 6 | Playback alignment and user controls | Not started | Lyric progression stays aligned; user can adjust offset, scrolling, pause and retry |
| 7 | Optional account-linked playback integration (Spotify capabilities/terms permitting) | Not started | Authentication and token handling are secure, user-initiated, and do not disrupt manual mic mode |
| 8 | Battery/charging and prolonged low-power/wake tests; remaining board peripherals (RTC, IMU, SD, speaker path if connector hardware available) | Not started | Each supported component passes independent physical check without compromising the core function |
| 9 | End-to-end regression, OTA updates/rollback/settings persistence | Not started | 100% pass of agreed core scenarios on actual device; no silent background listening |

Implementation sequence may be reprioritized after a failed hardware prerequisite, but never advance a failing prerequisite as “passed”. Source and host mocks alone do not certify a device peripheral.

### Stage 1 test procedure

1. Update to the `0.8.30-alpha` recovery build **only after CI compile/package pass**. Preserve saved settings through OTA; do not erase flash. First confirm full screen and touch, then repeat the ADC probe.
2. Visit device `/settings`, scroll to Diagnostics, tap **Test microphone controller** once.
3. Expect: `ES7210 detected`. In `/api/diagnostics`, `microphone_adc_status: "adc_detected"`, `microphone_adc_test_count >= 1`. Audio is still inactive and no sound is recorded.
4. Confirm Clock ↔ Home navigation, Wi-Fi, battery percentage and OTA interface still work. Report exact mic status and any change in display responsiveness.
5. If ADC absent/error, stop here. Investigate board variant and I2C conflicts before initializing audio. **No repeated firmware builds until issue is understood.**

## Deferred work / end-of-functional-cycle polish

### Top priority for the next OTA Web UI update — only 3 versions (user request)

**Status: NOT implemented. Keep this in the queue; do not modify the recovery build.**

- Replace the long OTA firmware dropdown with **at most three clearly labeled entries**: **Old** (one immediately previous published alpha relative to the installed device version), **Current** (actually installed firmware), **New** (latest published version newer than the installed one, when available).
- The old version remains **information-only/disabled** while OTA downgrades are unsupported; the installed current version is also not offered as an install action. Only a valid newer image can be installed via the existing verified OTA mechanism. Never imply the dropdown supports rollback.
- Use the device's `/api/info` installed version and version-aware release metadata to select the three entries; **do not simply take the newest three overall**. Omit an entry when it does not exist and avoid duplicating versions. Display no more than three entries and preserve refresh/status/error handling.
- Acceptance checks: test with installed version older than latest, equal to latest, with no preceding release, with a non-numeric/prerelease version, and with an empty or unreachable catalog. Keep local OTA upload and update safety guards unaffected.

- Display swipe hesitation observed on physical 0.8.26: `max_ui_tick_gap_ms=343`, `render_cycle_max_ms=291`; 0.8.28's larger LVGL PSRAM draw stripe caused a first-refresh stall on 0.8.29 hardware and was reverted in 0.8.30. Revisit only after lyrics and input path are operational.
- Improve swipe start threshold, touch/button hit-testing, page-settle transitions and occasional partial-page stalls. Retest after UI workload changes.
- Monitor for intermittent black Clock on return and green edge/line artifacts; mitigations exist, hardware regression tests remain necessary.
- Settings screen sometimes required Home instead of swipe-back; Clock/weather page exit and return navigation need consistent behavior.
- Wake after display sleep formerly caused stacked screens/unresponsive touches; regression check and defer cosmetic/layout fixes.
- Clock lag after color/ring changes and route through Clock button/tap vs swipe confusion: review late.
- Clock face artwork, loading polish and optional Rolex-style hand: **deferred, not included in functional test releases**.
- Source naming cleanup: remove remaining project-name references from the prior firmware, preserve legally necessary attribution in appropriate documentation.
- Audit any additional feedback discovered during hardware testing; append here without interrupting the current milestone.

## Engineering gates

- No automatic always-on mic: only manual Start may activate audio in a later stage; explicit Stop, elapsed timeout, network errors and sleep must deactivate capture.
- No provider keys or recording data in diagnostics or logs.
- Shared I2C bus must remain owned by Waveshare BSP; audio hardware detection must not disrupt touch (CST9217), battery PMIC (AXP2101), RTC or IMU.
- Keep private 0.x development and first official stable v1.0 separate; preserve OTA, Wi-Fi and settings in alpha-to-alpha updates.
- Keep one known-good rollback available and do not restart display-timing tuning during hardware milestones.
