#!/usr/bin/env python3
"""0.8.94: never finish a swipe based only on the target coordinate."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ui=(root/"main/ui.c").read_text()
h=(root/"main/ui.h").read_text()
net=(root/"main/network.c").read_text()
render=ui.split("static void display_render_event(lv_event_t *e)\n{",1)[1].split("static void display_render_ready_event",1)[0]
refresh=ui.split("static void display_refresh_ready_event(lv_event_t *e)\n{",1)[1].split("static void invalidate_full_screen",1)[0]
frame=ui.split("static void primary_pager_arrived(uint64_t current)\n{",1)[1].split("static void primary_pager_start_snap",1)[0]
watchdog=ui.split("static void primary_pager_watchdog(uint64_t current)\n{",1)[1].split("static void primary_pager_scroll_event",1)[0]
event=ui.split("static void primary_pager_scroll_event(lv_event_t *e)\n{",1)[1].split("static lv_obj_t *primary_page_create",1)[0]
assert "pager_render_scroll_x=(primary_pager && primary_pager_snapping) ?" in render
assert "pager_render_scroll_x=abs" not in refresh
assert "pager_final_frame_ready=true;" in refresh
assert "abs(pager_render_scroll_x-primary_pager_snap_to*466)<=2" in refresh
assert "pager_final_frame_acks,1" in refresh
assert "primary_pager_commit(primary_pager_snap_to);" in frame
assert "if(pager_final_frame_ready){" in frame
assert "if(!primary_pager_frame_pending && current-pager_arrived_at_ms>=240U){" in frame
assert "lv_obj_invalidate(primary_pager_pages[primary_pager_snap_to]);" in frame
assert "pager_final_frame_repaint_requests,1" in frame
assert "primary_pager_arrived(current);" in watchdog
assert "current-primary_pager_snap_started_ms>LF_PAGER_STALL_GUARD_MS" in watchdog
assert "primary_pager_arrived(now_ms());" in event
assert "primary_pager_commit(primary_pager_snap_to);" not in event
assert "pager_final_frame_ready=false;" in ui
assert "pager_arrived_at_ms=0;" in ui
assert "pager_final_frame_acks" in h and "pager_final_frame_repaint_requests" in h
for name in ("pager_final_frame_acks","pager_final_frame_repaint_requests"):
    assert f'"{name}"' in net
assert "lv_obj_scroll_to_x(primary_pager,target*466,LV_ANIM_ON);" in ui
assert "if(settled_move && !frame_presented && primary_pager_pages[index])" in ui
assert "pager_acknowledged_settles_no_full_repaint" in ui
assert "lf_network_ota_in_progress()" in (root/"main/main.c").read_text()
print("PASS: pager destination refresh acknowledgement, one-shot fallback, native scroll and recovery")
