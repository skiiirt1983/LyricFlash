#pragma once
/* 0.8.28: tested-diagnostics-driven PSRAM draw-buffer experiment.
 * Increase LVGL partial render height from 24 to 96 rows, targeting fewer
 * flushes per animation frame. Limit SPI bus transaction capacity to
 * its prior 24-row size so internal DMA RAM does not grow as a side effect.
 * Do NOT change four-row DMA staging, QSPI at 80MHz, queue depth 1,
 * 0.8.26 pager behavior, Clock repaint, OTA, power, Wi-Fi or NVS.
 * User must compare render_cycle_max_ms, max_ui_tick_gap_ms and flush count
 * on physical hardware. Base 0.8.26 retained intact for rollback. */
#define LF_VERSION "0.8.28-alpha"
#define LF_VERSION_LABEL "ALPHA 0.8.28"
