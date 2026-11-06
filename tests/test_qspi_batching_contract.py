#!/usr/bin/env python3
"""Validate LyricFlash 0.8.96 QSPI DMA batching, memory fallback and power baseline."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
base=root/"components/esp32_s3_touch_amoled_1_75"
bsp=(base/"esp32_s3_touch_amoled_1_75.c").read_text()
header=(base/"include/bsp/display.h").read_text()
net=(root/"main/network.c").read_text()
cfg=(root/"sdkconfig.defaults").read_text()
ui=(root/"main/ui.c").read_text()
assert "#define BSP_LCD_DMA_STAGING_ROWS (32)" in bsp
assert "#define BSP_LCD_DMA_SECONDARY_ROWS (16)" in bsp
assert "#define BSP_LCD_DMA_FALLBACK_ROWS (8)" in bsp
# 0.8.94 device reported a largest DMA-capable block of ~34.8 KB.
# A double 32-row buffer needs ~59.6 KB, whereas double 16-row fits.
panel_width=466
largest_reported_dma_block=34816
assert 32*panel_width*2*2 > largest_reported_dma_block
assert 16*panel_width*2*2 <= largest_reported_dma_block
assert bsp.index("max_rows=BSP_LCD_DMA_SECONDARY_ROWS;") < bsp.index("max_rows=BSP_LCD_DMA_FALLBACK_ROWS;")
assert "#define LVGL_BUFFER_HEIGHT_PSRAM (64)" in bsp
assert "uint16_t *new_buffer = heap_caps_malloc" in bsp
assert bsp.index("if(!new_buffer)return false;") < bsp.index("free(ctx->rotation_buffer);",bsp.index("static bool bsp_rotation_panel_ensure_buffer"))
assert "max_rows=BSP_LCD_DMA_SECONDARY_ROWS;" in bsp
assert "max_rows=BSP_LCD_DMA_FALLBACK_ROWS;" in bsp
assert "if(!bsp_rotation_panel_ensure_buffer(ctx,max_pixels*2))" in bsp
assert "atomic_fetch_add(&stage_chunk_submissions,1);" in bsp
assert "bsp_display_transfer_stats(bsp_display_transfer_stats_t *out)" in bsp
assert "bsp_display_transfer_stats_t" in header
for key in ("display_dma_stage_rows","display_dma_chunk_submissions","display_dma_stage_last_us","display_dma_stage_max_us","free_internal_dma_bytes","largest_internal_dma_block_bytes"):
    assert chr(34)+key+chr(34) in net
assert "CONFIG_BSP_LCD_TRANS_QUEUE_DEPTH=1" in cfg
assert ".tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE" in (root/"main/main.c").read_text()
assert "LF_PAGER_STALL_GUARD_MS 1600U" in ui
assert "lf_network_set_idle(true);" in ui and "lf_network_set_idle(false);" in ui
print("PASS: 0.8.96 64-row LVGL / 32-row DMA batching, 16-row/8-row memory fallbacks, unchanged safety/power")
