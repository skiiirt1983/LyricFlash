# LyricFlash 0.8.41-alpha — Manual recognition quality diagnostics

Private alpha only; not a public stable release. This update preserves the
0.8.40 large Play Home interface, established WAV capture, AudD request and
LRCLIB lookup logic. **No microphone gain or recognition-algorithm changes.**

The firmware measures both *actual* PCM channels being sent to AudD, directly
from ES7210 TDM slots 0 and 2 during the user-triggered 10-second scan.
Measurements are cumulative over that attempt and reset at scan start.

After a scan, open Device Studio → Identify a song, or the existing device
endpoint `/api/recognize`. The JSON includes:

- `audio_measured_frames`: 24 kHz frames represented by the measurements.
- `mic1_rms`, `mic2_rms`: integer full-scan RMS, 0–32768.
- `mic1_peak`, `mic2_peak`: peak absolute 16-bit sample, 0–32768.
- `mic1_rms_without_rail`, `mic2_rms_without_rail`: RMS with near-rail
  samples excluded, useful for identifying isolated transport spikes.
- `mic1_near_rail`, `mic2_near_rail`: sample counts with absolute amplitude
  at least 32760; `both_near_rail_frames` counts simultaneous channel hits.
- `mic1_half_scale`, `mic2_half_scale`: counts with amplitude at least 16384.
- `mic1_rail_bursts`, `mic2_rail_bursts`, `mic1_longest_rail_burst`,
  `mic2_longest_rail_burst`: detect contiguous near-rail runs, including
  runs spanning read-buffer boundaries.
- `mic_gain_db`: current fixed 24 dB microphone gain.
- `audd_http_status`, `audd_api_error_code`,
  `audd_transport_error_code`: help distinguish ordinary HTTP 200
  no-match decisions from provider or network failures.

A 10-second, 240,000-frame scan can contain samples at the rails without
proving clipping: bursts, RMS, placement and comparison with a recognized
control track matter more than one isolated peak. An HTTP 200 no-match is a
valid service result, not proof that the submitted WAV was good or bad.

## Physical A/B test

1. Update the experimental device via the existing OTA picker. Preserve NVS.
2. Play **Mr. Jones — Counting Crows**, a previously recognized control song,
   and manually scan for 10 seconds. Record the `/api/recognize` JSON.
3. Play one previously unrecognized popular song at the same volume and
   distance, and manually scan. Record the JSON.
4. Compare both microphone RMS/peak/near-rail counts and AudD HTTP status.
   Do not adjust mic gain until these results show a reproducible problem.
5. If AudD identifies a song but `lyrics_state` is `unavailable`, treat that
   as a separate LRCLIB/network issue; the lyrics implementation is untouched.

Privacy: The metrics are aggregates only. Raw audio is neither exposed through
the local status API nor stored for later export. Only explicit manual scans
activate AudD uploads, and existing stored tokens are never returned.
