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

## One item at a time — execution order

| Order | Stage | Status | Hardware pass criterion |
|---|---|---|---|
| 1 | Manual ES7210 audio-ADC I2C ACK probe via Web UI | **PASS: ES7210 electrical probe on 0.8.29 hardware; remaining display/touch gate pending 0.8.30** | Device Web UI reports `adc_detected` from address 0x40; battery, Wi-Fi, touch and clock still operate |
| 2 | On-demand ES7210 + I2S PCM amplitude test | Not started | User explicitly presses Start; live audio levels differ in quiet/noisy conditions; Stop reliably releases all capture resources; no audio uploaded |
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
