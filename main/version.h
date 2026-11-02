#pragma once
/* 0.8.17 experimental AMOLED isolation: disables TE wait after 0.8.16
   blocked on second display flush, retains PSRAM DMA staging, QSPI queue
   depth one and core-1 LVGL. No changes to NVS, Wi-Fi, OTA or partitions.
   Hardware validation still required. */
#define LF_VERSION "0.8.17-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.17"
/* 0.8.17 TE-off compile checkpoint: all source preflight assertions passed. */
