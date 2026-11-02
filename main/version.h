#pragma once
/* 0.8.22 experimental native, finger-following Clock/Home/Settings pager.
   Three persistent page CHILDREN of ONE LVGL root; no second LVGL screen.
   Clock scene cache preserved; only the main menu swipes natively;
   Weather/submenus remain on the established deferred navigation path.
   CO5300 QSPI DMA-staging driver, TE disabled, transaction queue depth 1,
   LVGL core 1, OTA and NVS/partitions remain unchanged.
   Hardware gesture smoothness, battery/wake and no-stacking untested. */
#define LF_VERSION "0.8.22-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.22"
