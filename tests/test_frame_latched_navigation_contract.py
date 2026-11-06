#!/usr/bin/env python3
"""0.8.89: LVGL native pager easing with finger safety and bounded stall recovery."""
from pathlib import Path
ui=(Path(__file__).resolve().parents[1]/"main/ui.c").read_text()
def section(start,end):
    assert start in ui and end in ui, (start,end)
    return ui.split(start,1)[1].split(end,1)[0]
frame=section("static void primary_pager_snap_frame(lv_timer_t *timer)","static void primary_pager_start_snap(int target)")
start=section("static void primary_pager_start_snap(int target)","static int primary_pager_target(int32_t x)")
watchdog=section("static void primary_pager_watchdog(uint64_t current)","static void primary_pager_scroll_event")
gesture=section("static void primary_pager_scroll_event(lv_event_t *e)","static lv_obj_t *primary_page_create")
builder=section("static void primary_pager_create(int target_index)","static void primary_pager_enter(int index)")
destruct=section("static void make_base(void)","static void on_wifi(")
commit=section("static void primary_pager_commit(int index)","/* Native LVGL easing")
nav_tick=section("static void nav_transition_tick(lv_timer_t *timer)","static void nav_transition_begin")
nav_begin=section("static void nav_transition_begin(lv_screen_load_anim_t direction)","/* Deferred scene construction")
assert "LF_PAGER_STALL_GUARD_MS 1600U" in ui
assert "LF_PAGER_MAX_FRAME_STEP_PX" not in ui
assert "LF_PAGER_SNAP_DURATION_MS" not in ui
assert "if(ui_pointer_is_down()){" in frame
assert "lv_timer_pause(primary_pager_frame_timer);" in frame
assert "primary_pager_arrived(now_ms());" in frame
assert "if(pager_final_frame_ready){" in ui
assert "primary_pager_commit(primary_pager_snap_to);" in ui
assert "lv_obj_scroll_to_x(primary_pager,next,LV_ANIM_OFF);" not in ui
assert "lv_obj_scroll_to_x(primary_pager,target*466,LV_ANIM_ON);" in start
assert "lv_timer_resume(primary_pager_frame_timer);" in start
assert "primary_pager_snapping=true;" in start
assert "lv_anim_set_path_cb(anim,lv_anim_path_ease_out)" in gesture
assert "lv_anim_set_duration(anim,185U+(uint32_t)(105U*remaining/466U));" in gesture
assert "current-primary_pager_snap_started_ms>LF_PAGER_STALL_GUARD_MS" in watchdog
assert "lv_obj_scroll_to_x(primary_pager,target*466,LV_ANIM_OFF);" in watchdog
assert "lv_indev_get_state(indev)==LV_INDEV_STATE_PRESSED)return;" in watchdog
assert "primary_pager_frame_pending=false;" in gesture
assert "lv_timer_pause(primary_pager_frame_timer);" in gesture
assert "primary_pager_frame_timer=lv_timer_create(primary_pager_snap_frame,16,NULL);" in builder
assert "lv_timer_delete(primary_pager_frame_timer);" in destruct
assert "primary_pager_frame_timer=NULL;" in destruct
assert "primary_pager_frame_pending=false;" in commit
assert "lv_timer_pause(primary_pager_frame_timer);" in commit
assert "atomic_load(&ui_render_count)==nav_transition_frame_render_at_issue" in nav_tick
assert "atomic_load(&ui_refresh_ready_count)==nav_transition_frame_ready_at_issue" in nav_tick
assert "nav_transition_frame_pending=true;" in nav_tick
assert "nav_transition_frame_pending=false;" in nav_begin
assert "lv_obj_set_scroll_dir(primary_pager,LV_DIR_HOR);" in builder
assert "lv_obj_remove_flag(primary_pager,LV_OBJ_FLAG_SCROLL_ELASTIC);" in builder
assert "lv_obj_remove_flag(primary_pager,LV_OBJ_FLAG_SCROLL_CHAIN_HOR);" in builder
print("PASS: native LVGL elapsed-time pager, press interruption, stall guard, single-root and secondary scene safety")
