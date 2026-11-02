#pragma once
/* 0.8.23 experimental Clock/Home/Settings pager stabilization.
   One deterministic LVGL snap with timeout recovery for missed scroll-end
   events, and an OPTIONAL 16-bit static clock snapshot during page motion
   to avoid repainting the detailed dial every frame. Snapshot failure keeps
   original live clock; live scene restored after paging. Retains 0.8.22's
   single-root touchscreen and the 0.8.19 fast Basic Clock cache.
   CO5300 QSPI DMA staging, TE off, queue=1, core=1 unchanged.
   OTA, Wi-Fi, power, settings and partitions unchanged.
   Hardware performance and input safety still require testing. */
#define LF_VERSION "0.8.23-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.23"
/* 0.8.23 snap cache compile checkpoint: preflight green; driver unchanged. */
