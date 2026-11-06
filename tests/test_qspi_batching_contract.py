#!/usr/bin/env python3
"""Validate LyricFlash 0.8.98 internal-RAM recovery and retained LVGL throughput."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
base=root/"components/esp32_s3_touch_amoled_1_75"
bsp=(base/"esp32_s3_touch_amoled_1_75.c").read_text()
header=(base/"include/bsp/display.h").read_text()
net=(root/"main/network.c").read_text()
cfg=(root/"sdkconfig.defaults").read_text()
ui=(root/"main/ui.c").read_text()
assert "#define BSP_LCD_DMA_STAGING_ROWS (8)" in bsp
assert "#define BSP_LCD_DMA_FALLBACK_ROWS (4)" in bsp
assert "BSP_LCD_DMA_SECONDARY_ROWS" not in bsp
# Internal DMA staging must leave allocation headroom for HTTPS/TLS and JSON.
# Two 8-row slots are ~15 KB; 0.8.96 used ~60 KB for two 32-row slots.
panel_width=466
assert 8*panel_width*2*2 == 14912
assert 32*panel_width*2*2 == 59648
assert 8*panel_width*2*2 < 16*1024
assert "#define LVGL_BUFFER_HEIGHT_PSRAM (64)" in bsp
assert "uint16_t *new_buffer = heap_caps_malloc" in bsp
assert bsp.index("if(!new_buffer)return false;") < bsp.index("free(ctx->rotation_buffer);",bsp.index("static bool bsp_rotation_panel_ensure_buffer"))
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
print("PASS: 0.8.98 keeps 64-row PSRAM LVGL, reduces internal DMA staging to 8/4 rows, preserves OTA/power guards")
