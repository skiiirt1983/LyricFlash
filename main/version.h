#pragma once
/* 0.8.30: revert the untested 96-row LVGL render-buffer experiment
 * after physical 0.8.29 testing showed white screen/no touch and first
 * display refresh stalled (2 starts, 1 finish, 0 refresh-ready).
 * Exact 0.8.26 hardware-proven display-driver file is restored.
 * Keep ES7210 on-demand hardware probe and Web UI (detected on board).
 * No changes to mic capture (remains off), OTA, Wi-Fi, NVS or power. */
#define LF_VERSION "0.8.30-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.30"
