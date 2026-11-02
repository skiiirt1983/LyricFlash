#pragma once
/* 0.8.26: shorten distance-scaled LVGL pager snaps, disable scroll momentum
   on the 3-page pager (without changing live finger-following), and expose
   pager snap / render-cycle timing to the local diagnostics endpoint.
   Preserve 0.8.25's real visible Clock, forced-snap watchdog and repaint fix.
   Keep QSPI 80MHz, four-row DMA, TE OFF, Wi-Fi/OTA, NVS and partition layout
   unchanged. Hardware smoothness and memory stability require validation. */
#define LF_VERSION "0.8.26-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.26"
