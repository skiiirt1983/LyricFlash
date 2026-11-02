#pragma once
/* 0.8.27: small pager feel refinement after the user confirmed 0.8.26
   is nearly perfect on physical hardware. Reduce only the terminal
   distance-aware snap range from 95-180ms to 85-155ms. The finger-owned
   native LVGL scroll, live Clock repaint, 700ms watchdog and diagnostic
   counters remain unchanged. Preserve the 0.8.26 branch as rollback.
   No changes to display DMA, OTA, Wi-Fi, NVS, power or partitions. */
#define LF_VERSION "0.8.27-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.27"
