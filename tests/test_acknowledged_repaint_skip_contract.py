#!/usr/bin/env python3
"""0.8.95: only skip a redundant full page refresh when the final frame was acknowledged."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ui=(root/"main/ui.c").read_text()
head=(root/"main/ui.h").read_text()
net=(root/"main/network.c").read_text()
commit=ui.split("static void primary_pager_commit(int index)\n{",1)[1].split("/* Native LVGL easing",1)[0]
assert "const bool frame_presented=primary_pager_snapping && pager_final_frame_ready &&" in commit
assert "pager_render_scroll_x==-1;" in commit
assert commit.index("const bool frame_presented=")<commit.index("pager_final_frame_ready=false;")
assert "if(settled_move && !frame_presented && primary_pager_pages[index])" in commit
assert "lv_obj_invalidate(primary_pager_pages[index]);" in commit
assert "if(settled_move && frame_presented)" in commit
assert "atomic_fetch_add(&pager_acknowledged_settles_no_full_repaint,1);" in commit
assert "if(!settled_move)lv_obj_invalidate(clock_scene);" in commit
assert "lf_clock_update();" in commit
assert "lv_obj_remove_flag(clock_scene,LV_OBJ_FLAG_HIDDEN);" in commit
assert "if(index==1)home_refresh(now_ms());" in commit
assert "pager_acknowledged_settles_no_full_repaint" in head
assert '"pager_acknowledged_settles_no_full_repaint"' in net
assert "pager_final_frame_acks" in head and "pager_final_frame_repaint_requests" in head
assert "if(primary_pager_snapping && primary_pager && pager_render_scroll_x>=0" in ui
assert "pager_final_frame_ready=true;" in ui
assert "current-primary_pager_snap_started_ms>LF_PAGER_STALL_GUARD_MS" in ui
assert "LVGL_BUFFER_HEIGHT_PSRAM (64)" in (root/"components/esp32_s3_touch_amoled_1_75/esp32_s3_touch_amoled_1_75.c").read_text()
assert "CONFIG_BSP_LCD_TRANS_QUEUE_DEPTH=1" in (root/"sdkconfig.defaults").read_text()
print("PASS: ACK-confirmed destinations omit second full repaint, unconfirmed still protected, QSPI queue safety preserved with larger display bands")
