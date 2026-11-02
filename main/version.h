#pragma once
/* 0.8.25 diagnostic recovery: fix intermittent black Clock on return
   from Home reported on 0.8.24. Remove the optional RGB565 snapshot proxy
   that swapped hidden/live clock objects during swipes, and explicitly
   invalidate the live Clock when the native pager settles.
   Keep 0.8.23's finger-following pages and stuck-snap watchdog.
   Revert the ineffective 0.8.24 QSPI batching experiment to the proven
   four-row DMA staging: CO5300 80MHz, TE OFF, queue depth 1.
   No changes to battery, OTA, Wi-Fi, NVS or partitions.
   Hardware visibility, swipes and memory stability must be tested. */
#define LF_VERSION "0.8.25-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.25"
