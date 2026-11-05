#!/usr/bin/env python3
"""0.8.76: never restrict physical return swipe; preserve timeout guards."""
from pathlib import Path
ui=(Path(__file__).resolve().parents[1]/"main/ui.c").read_text()
commit=ui.split("static void primary_pager_commit(int index)\n{",1)[1].split("/* PrintSphere-like LVGL native elapsed-time easing",1)[0]
create=ui.split("static void primary_pager_create(int target_index)\n{",1)[1].split("static void primary_pager_enter",1)[0]
scroll=ui.split("static void primary_pager_scroll_event(lv_event_t *e)\n{",1)[1].split("static lv_obj_t *primary_page_create",1)[0]
sleep=ui.split("static void sleep_display(void)\n{",1)[1].split("static void on_tick",1)[0]
tick=ui.split("static void on_tick_body(lv_timer_t *timer)\n{",1)[1].split("/* Only the active Rolex-style",1)[0]
assert "lv_obj_set_scroll_dir(primary_pager," not in commit, "Committing a page must not restrict return swipes"
assert "LV_DIR_HOR" not in commit, "No per-page direction change allowed"
assert "static void primary_pager_start_snap(int target)" in ui
assert "lv_obj_scroll_to_x(primary_pager,target*466,LV_ANIM_ON);" in ui  # native snap
assert "lv_obj_scroll_to_x(primary_pager,next,LV_ANIM_OFF);" not in ui
assert "primary_pager_frame_timer" in ui
assert "primary_pager_frame_pending" in ui
assert "primary_pager_snap_frame" in ui
assert "lv_obj_remove_flag(primary_pager,LV_OBJ_FLAG_SCROLL_ELASTIC);" in create
assert "lv_obj_remove_flag(primary_pager,LV_OBJ_FLAG_SCROLL_CHAIN_HOR);" in create
assert "lv_obj_set_scroll_dir(primary_pager,LV_DIR_HOR);" in create
assert "lv_obj_set_style_bg_color(primary_pager," in create, "Pager background must not expose white beyond Settings"
assert "lv_color_hex(prefs->background_color),0);" in create
assert "lv_obj_remove_flag(primary_pager,LV_OBJ_FLAG_SCROLL_MOMENTUM);" in create
assert scroll.count("activity();")>=3, "Physical and animated pager movement must reset inactivity"
assert "pager_scrolling ||" in sleep and "primary_pager_snapping || nav_transition_active)return;" in sleep
assert "!pager_scrolling && !primary_pager_snapping && !nav_transition_active &&" in tick
assert "!asleep && ui_pointer_is_down()" in tick
assert "lv_obj_set_style_bg_opa(sleep_overlay,LV_OPA_TRANSP,LV_STATE_PRESSED);" in ui
assert "wake_cleanup_queued" in ui
assert "lv_timer_resume(clock_sweep_timer);" in ui
print("PASS: bidirectional Settings→Home and Clock→Home pager retained with no elastic/chaining and no mid-swipe timeout")
