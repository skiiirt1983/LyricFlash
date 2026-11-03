#pragma once
/* Alpha 0.8.35: permit one-click confirmed SAME-VERSION OTA reinstall.
 * Firmware Web UI still shows a maximum of 3 relative rows: Old disabled,
 * Current enabled ONLY if an exact matching published verified OTA exists,
 * New enabled if a newer published verified image exists. A reinstall does
 * NOT erase saved Wi-Fi, NVS preferences or device settings.
 * Unknown/missing catalog and unparseable versions fail closed. The on-device
 * ESP-IDF updater verifies image project and exact requested image version,
 * and does not reject the installed version. Manual OTA and USB recovery are
 * separate. All 0.8.33 microphones, QSPI display, clock and touch unchanged.
 * Physical UI/reinstall verification required before stable promotion. */
#define LF_VERSION "0.8.35-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.35"
