#!/usr/bin/env python3
"""0.8.88 analogue clock redraw budget and rollback-safe display configuration."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
c=(root/"main/clock_face.c").read_text()
bsp=(root/"components/esp32_s3_touch_amoled_1_75/esp32_s3_touch_amoled_1_75.c").read_text()
cfg=(root/"sdkconfig.defaults").read_text()
ui=(root/"main/ui.c").read_text()
net=(root/"main/network.c").read_text()
assert "typedef struct {" in c and "lf_dial_tick_t" in c
assert "static lf_dial_tick_t dial_tick_cache[2][60];" in c
assert "static void dial_ticks_prepare(bool diver)" in c
assert "if(dial_ticks_ready[style])return;" in c
assert "dial_ticks_prepare(diver);" in c
draw=c[c.index("static void tick_draw_event"):c.index("static void draw_ticks")]
assert "cosf(" not in draw and "sinf(" not in draw
assert "ticks[i].x1" in draw and "lv_draw_line(layer,&line);" in draw
assert "static void update_hand_if_moved" in c
assert "if(hand_points[index][0].x==ends[0].x" in c
assert "update_hand_if_moved(2,angle,164);" in c
assert "sundial_shadow_points[1].x!=new_shadow[1].x" in c
assert "# CONFIG_LV_OBJ_STYLE_CACHE is not set" in cfg
assert "#define LVGL_BUFFER_HEIGHT_PSRAM (24)" in bsp
assert "#define BSP_LCD_DMA_STAGING_ROWS (8)" in bsp
assert "#define BSP_LCD_DMA_FALLBACK_ROWS (4)" in bsp
assert "CONFIG_BSP_LCD_TRANS_QUEUE_DEPTH=1" in cfg
assert "stage_cumulative_us" in bsp
assert "render_dma_stage_last_us" in ui and chr(34)+"render_dma_stage_last_us"+chr(34) in net
assert "lf_network_set_idle(true);" in ui and "lf_network_set_idle(false);" in ui
assert "LF_PAGER_MAX_FRAME_STEP_PX 58" in ui
print("PASS: clock tick trig memoized, hand pixel-change gating, style cache off, baseline DMA/power preserved")
