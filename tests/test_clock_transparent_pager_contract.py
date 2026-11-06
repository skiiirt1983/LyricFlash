#!/usr/bin/env python3
"""0.8.97: the live round Clock may not become a sliding opaque square."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ui=(root/"main/ui.c").read_text()
clock=(root/"main/clock_face.c").read_text()
basic=(root/"main/basic_clock.c").read_text()
bsp=(root/"components/esp32_s3_touch_amoled_1_75/esp32_s3_touch_amoled_1_75.c").read_text()
scene=ui.split("static void primary_pager_build_clock(void)\n{",1)[1].split("static void primary_pager_create(",1)[0]
face=clock.split("void lf_clock_build(lv_obj_t *parent, uint8_t style, const lf_settings_t *settings)\n{",1)[1].split("void lf_clock_update(void)",1)[0]
page=ui.split("static lv_obj_t *primary_page_create(lv_obj_t *pager,int index)\n{",1)[1].split("static void primary_pager_build_clock",1)[0]
assert "lv_obj_set_style_bg_opa(clock_scene,LV_OPA_TRANSP,0);" in scene
assert "lv_obj_set_style_bg_opa(clock_scene,LV_OPA_COVER,0);" not in scene
assert "lv_obj_set_style_bg_opa(parent, LV_OPA_TRANSP, 0);" in face
assert "lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);" not in face
# Both analog and digital watch faces keep their round bezel/dial backgrounds.
assert "round_panel(parent, 452" in clock and "round_panel(parent, 447" in clock
# The fully custom Basic face keeps its user-selected colour within a circle.
assert "round_panel(parent,452,settings->basic_background" in face
assert face.index("round_panel(parent,452,settings->basic_background") < face.index("lf_basic_clock_render(parent, settings)")
assert "lv_obj_set_style_bg_color(parent, lv_color_hex(p->basic_background), 0);" in basic
# A real opaque page still supplies the common background outside the dial:
# do not expose uninitialized AMOLED panel memory or regress blank-page recovery.
assert "lv_obj_set_style_bg_opa(p,LV_OPA_COVER,0);" in page
assert "if(settled_move && !frame_presented && primary_pager_pages[index])" in ui
assert "lv_obj_scroll_to_x(primary_pager,target*466,LV_ANIM_ON);" in ui
# Preserve the physically improved 0.8.96 banding and adaptive DMA fallback.
for gate in ("#define LVGL_BUFFER_HEIGHT_PSRAM (64)",
             "#define BSP_LCD_DMA_STAGING_ROWS (32)",
             "#define BSP_LCD_DMA_SECONDARY_ROWS (16)",
             "#define BSP_LCD_DMA_FALLBACK_ROWS (8)"):
    assert gate in bsp,gate
print("PASS: round transparent Clock pager retains user face colours, redraw guards and 0.8.96 display throughput")
