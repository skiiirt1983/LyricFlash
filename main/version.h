#pragma once
/* Alpha 0.8.38: After a manually requested AudD song identification, query
 * LRCLIB over verified HTTPS using title/artist only (no mic audio or key).
 * Timestamp-parse synced lyrics into PSRAM and show count + four-line preview
 * in Device Settings. This is not yet real-time playback synchronization.
 * One version-neutral title fallback; no automatic 429 retries.
 * Optional Pages catalog 404 is hidden from end users; total update-check
 * failure remains a clear nontechnical message. Preserve 24kHz MIC/24dB,
 * QSPI/LVGL clock/touch, NVS, and Current-version OTA reinstall.
 * Hardware lyrics lookup and Stop validation required. */
#define LF_VERSION "0.8.38-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.38"
