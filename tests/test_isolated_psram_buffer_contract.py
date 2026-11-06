#!/usr/bin/env python3
"""0.8.99: isolate partial draw-buffer size while preserving 0.8.95 DMA/network."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
bsp=(root/"components/esp32_s3_touch_amoled_1_75/esp32_s3_touch_amoled_1_75.c").read_text()
version=(root/"main/version.h").read_text()
cfg=(root/"sdkconfig.defaults").read_text()
assert '#define LF_VERSION "0.8.99-alpha"' in version
assert "#define LVGL_BUFFER_HEIGHT_PSRAM (32)" in bsp
assert "#define BSP_LCD_DMA_STAGING_ROWS (8)" in bsp
assert "#define BSP_LCD_DMA_FALLBACK_ROWS (4)" in bsp
assert "height > BSP_LCD_DMA_STAGING_ROWS;" in bsp
assert "BSP_LCD_DMA_SECONDARY_ROWS" not in bsp
assert "max_rows=BSP_LCD_DMA_FALLBACK_ROWS;" in bsp
assert "CONFIG_BSP_LCD_TRANS_QUEUE_DEPTH=1" in cfg
assert "BSP_LCD_H_RES * buffer_height * BSP_LCD_BITS_PER_PIXEL / 8" in bsp
print("PASS: 0.8.99 tests ONLY a 32-row PSRAM draw buffer; 0.8.95 internal DMA bounds intact")
