#pragma once
/* 0.8.21 diagnostic refinement: remove only the visible green accent
   divider from the eased directional panel-paced transition. The actual
   strip reveal and single-root input catcher remain unchanged.
   Preserve known-working CO5300 DMA staging, TE OFF, QSPI queue=1,
   LVGL core 1 and 0.8.19 cached Basic Clock; no OTA/NVS changes.
   Untested on hardware: appearance and touch still require confirmation. */
#define LF_VERSION "0.8.21-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.21"
/* 0.8.21 green-line compile checkpoint: source tests passed; QSPI driver unchanged. */
