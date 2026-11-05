#!/usr/bin/env python3
"""0.8.85: verify the LVGL drawing vs flush-wait instrumentation."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ui=(root/"main/ui.c").read_text()
net=(root/"main/network.c").read_text()
header=(root/"main/ui.h").read_text()
settings=(root/"main/settings.c").read_text()
assert ".sleep_seconds = 60," in settings
assert "lf_network_set_idle(true);" in ui
assert "lf_network_set_idle(false);" in ui
events=[("LV_EVENT_RENDER_READY","display_render_ready_event"),("LV_EVENT_FLUSH_WAIT_START","display_flush_wait_start_event"),("LV_EVENT_FLUSH_WAIT_FINISH","display_flush_wait_finish_event")]
for event,cb in events:
    assert "lv_display_add_event_cb(disp,"+cb+","+event+",NULL);" in ui
assert "draw_started_us=render_cycle_start_us;" in ui
assert "flush_wait_accum_us" in ui and "flush_calls_in_refresh" in ui
fields=("render_draw_last_us","render_draw_max_us","flush_wait_refresh_last_us","flush_wait_refresh_max_us","flush_calls_refresh_last","flush_calls_refresh_max")
for key in fields:
    assert key in ui and key in header
    assert chr(34)+key+chr(34) in net
home=ui.split("static void home_refresh(uint64_t current)\n{",1)[1].split("static void primary_render_home_content(",1)[0]
assert "lv_obj_has_flag(home_song_title,LV_OBJ_FLAG_HIDDEN)!=hide_title" in home
assert "if(hide_title)lv_obj_add_flag(home_song_title,LV_OBJ_FLAG_HIDDEN);" in home
assert "primary_pager_frame_pending=true;" in ui
assert "LF_PAGER_STALL_GUARD_MS 1600U" in ui
print("PASS: LVGL draw/wait metrics and guarded Home invalidation preserve baseline")
