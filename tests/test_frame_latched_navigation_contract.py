#!/usr/bin/env python3
"""0.8.81: ensure page motion waits for completed renders, not just a wall clock."""
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
commit=section("static void primary_pager_commit(int index)","/* Present one actual position")
nav_tick=section("static void nav_transition_tick(lv_timer_t *timer)","static void nav_transition_begin")
nav_begin=section("static void nav_transition_begin(lv_screen_load_anim_t direction)","/* Deferred scene construction")
assert "LF_PAGER_FRAME_MIN_MS 28U" in ui
assert "LF_PAGER_MAX_FRAME_STEP_PX 58" in ui
assert "LF_PAGER_SNAP_DURATION_MS 430U" in ui
assert "LF_PAGER_STALL_GUARD_MS 1600U" in ui
assert "if(ui_pointer_is_down()){" in frame
assert "lv_timer_pause(primary_pager_frame_timer);" in frame
assert "atomic_load(&ui_render_count)==primary_pager_frame_render_at_issue" in frame
assert "atomic_load(&ui_refresh_ready_count)==primary_pager_frame_ready_at_issue" in frame
assert "current-primary_pager_last_frame_ms<LF_PAGER_FRAME_MIN_MS" in frame
assert "if(advance>LF_PAGER_MAX_FRAME_STEP_PX)" in frame
assert "if(advance< -LF_PAGER_MAX_FRAME_STEP_PX)" in frame
assert "primary_pager_frame_pending=true;" in frame
assert frame.index("primary_pager_frame_pending=true;")<frame.index("lv_obj_scroll_to_x(primary_pager,next,LV_ANIM_OFF);")
assert "lv_timer_reset(primary_pager_frame_timer);" in start
assert "lv_timer_resume(primary_pager_frame_timer);" in start
assert 'lv_obj_scroll_to_x(primary_pager,target*466,LV_ANIM_ON);' in start  # only allocation-failure fallback
assert "current-primary_pager_snap_started_ms>LF_PAGER_STALL_GUARD_MS" in watchdog
assert watchdog.index("current-primary_pager_snap_started_ms>LF_PAGER_STALL_GUARD_MS") < watchdog.index("if(abs(x-primary_pager_snap_to*466)<=2)")
assert "!primary_pager_frame_pending" in watchdog
assert "!primary_pager_frame_pending" in gesture
assert "primary_pager_frame_pending=false;" in gesture  # input interruption
assert "lv_timer_pause(primary_pager_frame_timer);" in gesture
assert "primary_pager_frame_timer=lv_timer_create(primary_pager_snap_frame,16,NULL);" in builder
assert "lv_timer_delete(primary_pager_frame_timer);" in destruct
assert "primary_pager_frame_timer=NULL;" in destruct
assert "primary_pager_frame_pending=false;" in commit
assert "lv_timer_pause(primary_pager_frame_timer);" in commit
# Secondary menu reveal must wait until a real render has completed.
assert "atomic_load(&ui_render_count)==nav_transition_frame_render_at_issue" in nav_tick
assert "atomic_load(&ui_refresh_ready_count)==nav_transition_frame_ready_at_issue" in nav_tick
assert "nav_transition_frame_pending=true;" in nav_tick
assert nav_tick.index("nav_transition_frame_pending=true;")<nav_tick.index("lv_obj_invalidate_area(screen,&region);")
assert "nav_transition_step>=LF_NAV_REVEAL_STEPS" in nav_tick
assert "nav_transition_end(true);" in nav_tick
assert "nav_transition_frame_pending=false;" in nav_begin
assert "if(next==LF_NAV_PANEL_SIZE)nav_transition_end(false);" not in nav_tick
# The scroll bounds and finger tracking are not replaced by another pager.
assert "lv_obj_set_scroll_dir(primary_pager,LV_DIR_HOR);" in builder
assert "lv_obj_remove_flag(primary_pager,LV_OBJ_FLAG_SCROLL_ELASTIC);" in builder
assert "lv_obj_remove_flag(primary_pager,LV_OBJ_FLAG_SCROLL_CHAIN_HOR);" in builder
# Independent arithmetic check for 58px max displacement per presented frame.
def move(frm,dest,elapsed):
    t=min(elapsed,430)*1024//430
    eased=t*t*(3072-2*t)//(1024*1024)
    desired=frm+(dest-frm)*eased//1024
    d=max(-58,min(58,desired-frm))
    if d==0 and frm!=dest: d=1 if dest>frm else -1
    return frm+d
for dest in [0,466,932]:
    for frm in [0,130,466,720,932]:
        if frm==dest: continue
        x=frm
        for i in range(40):
            next_x=move(x,dest,430)
            if dest>x:next_x=min(next_x,dest)
            else:next_x=max(next_x,dest)
            assert abs(next_x-x)<=58
            assert min(frm,dest)<=next_x<=max(frm,dest)
            x=next_x
            if x==dest:break
        assert x==dest, (frm,dest)
print("PASS: post-release pager motion bounded per rendered frame; screen reveal waits for refresh and cannot trap touch")
