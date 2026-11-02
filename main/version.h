#pragma once
/* 0.8.24 alpha: reduce QSPI DMA submission overhead during a sliding pager.
   Prefer 24-row staging per 24-row LVGL render tile, with memory-guarded
   16/8/4-row fallback. Allocate before first DMA transfer, hold two slots.
   CO5300 transport still uses 80MHz QSPI, TE OFF, queue depth 1.
   0.8.23 snap recovery, optional RGB565 Clock image, one LVGL root,
   OTA, Wi-Fi, power, NVS, settings and partition layout unchanged.
   Requires physical smoothness, stability, and memory-pressure testing. */
#define LF_VERSION "0.8.24-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.24"
/* 0.8.24 adaptive DMA compile checkpoint: source checks and diff audit passed. */
