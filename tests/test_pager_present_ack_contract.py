#!/usr/bin/env python3
"""0.8.95 regression: remove ineffective 0.8.94 destination-frame ACK gate."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ui=(root/"main/ui.c").read_text()
header=(root/"main/ui.h").read_text()
net=(root/"main/network.c").read_text()
frame=ui.split("static void primary_pager_snap_frame(lv_timer_t *timer)\n{",1)[1].split("static void primary_pager_start_snap",1)[0]
watchdog=ui.split("static void primary_pager_watchdog(uint64_t current)\n{",1)[1].split("static void primary_pager_scroll_event",1)[0]
event=ui.split("static void primary_pager_scroll_event(lv_event_t *e)\n{",1)[1].split("static lv_obj_t *primary_page_create",1)[0]
assert "primary_pager_commit(primary_pager_snap_to);" in frame
assert "if(!primary_pager_frame_pending)" in watchdog
assert "primary_pager_commit(primary_pager_snap_to);" in watchdog
assert "primary_pager_commit(primary_pager_snap_to);" in event
assert "primary_pager_frame_pending=false;" in ui
assert "lv_obj_scroll_to_x(primary_pager,target*466,LV_ANIM_ON);" in ui
assert "LF_PAGER_STALL_GUARD_MS 1600U" in ui
assert "lv_obj_invalidate(primary_pager_pages[index]);" in ui
for name in ("pager_final_frame_acks","pager_final_frame_repaint_requests","pager_final_frame_ready","pager_render_scroll_x","pager_arrived_at_ms","primary_pager_arrived"):
    assert name not in ui,name
    assert name not in header,name
    assert name not in net,name
# Retain 0.8.93 on-device improved draw culling.
basic=(root/"main/basic_clock.c").read_text()
clock=(root/"main/clock_face.c").read_text()
assert "const lv_area_t *clip=&layer->_clip_area;" in basic
assert "const lv_area_t *clip=&layer->_clip_area;" in clock
# 0.8.94 Copy Log fix is retained, not reverted with the pager.
web=(root/"main/web_device_html.h").read_text()
assert "function lfSelectFullLog()" in web
assert "function lfCopyFallback(text)" in web
assert "left:-9999px" not in web
print("PASS: 0.8.94 pager ACK completely reverted; 0.8.93 clock + WebUI copy correction preserved")
