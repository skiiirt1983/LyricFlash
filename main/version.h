#pragma once
/* Alpha 0.8.34: device Web UI firmware picker shows a maximum of 3 entries:
 * Old (nearest published version below installed, disabled), Current
 * (actual running firmware, disabled), New (latest newer published OTA,
 * user-selectable). Unrecognized installed versions fail closed. Keeps
 * existing verified Alpha/Beta catalog, direct GitHub OTA URL validation,
 * manual file-upload fallback, OTA HTTPS, save state and reboot semantics.
 * Audio driver and mic gain, LVGL/QSPI display, touch, Wi-Fi, NVS all
 * remain identical to 0.8.33, which physically passed dual-mic signal
 * response testing (rare near-rail samples remain under investigation).
 * Hardware test required before promoting as an installed baseline. */
#define LF_VERSION "0.8.34-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.34"
