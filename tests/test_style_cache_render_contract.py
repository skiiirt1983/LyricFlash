#!/usr/bin/env python3
"""0.8.87: isolate LVGL style caching and measure real per-render DMA submissions."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
base=root/"components/esp32_s3_touch_amoled_1_75"
bsp=(base/"esp32_s3_touch_amoled_1_75.c").read_text()
head=(base/"include/bsp/display.h").read_text()
cfg=(root/"sdkconfig.defaults").read_text()
ui=(root/"main/ui.c").read_text()
ui_head=(root/"main/ui.h").read_text()
net=(root/"main/network.c").read_text()
assert "CONFIG_LV_OBJ_STYLE_CACHE=y" in cfg
assert "#define LVGL_BUFFER_HEIGHT_PSRAM (24)" in bsp
assert "#define BSP_LCD_DMA_STAGING_ROWS (4)" in bsp
assert "BSP_LCD_DMA_FALLBACK_ROWS" not in bsp
assert "CONFIG_BSP_LCD_TRANS_QUEUE_DEPTH=1" in cfg
assert "stage_cumulative_us" in bsp and "stage_cumulative_us" in head
assert "atomic_fetch_add(&stage_cumulative_us,duration);" in bsp
assert "render_dma_stage_at_start_us=staged.stage_cumulative_us;" in ui
assert "const uint32_t stage_us=staged.stage_cumulative_us-render_dma_stage_at_start_us;" in ui
for key in ("render_dma_stage_last_us","render_dma_stage_max_us"):
    assert key in ui_head and chr(34)+key+chr(34) in net
assert "LF_PAGER_MAX_FRAME_STEP_PX 58" in ui
assert "lf_network_set_idle(true);" in ui and "lf_network_set_idle(false);" in ui
assert "bsp_display_start_direct()" in (root/"main/main.c").read_text()
print("PASS: LVGL 9 style cache, physical-stage render timing, 24-row/4-row display and 1-slot SPI safety")
