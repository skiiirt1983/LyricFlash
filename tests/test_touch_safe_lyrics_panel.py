#!/usr/bin/env python3
"""Touch-safe manual capture and reusable vertical Lyrics panel UI contracts."""
from pathlib import Path
src=(Path(__file__).resolve().parents[1]/"main/ui.c").read_text()
def section(a,b):
    return src.split(a,1)[1].split(b,1)[0]
home=section("static void primary_render_home_content(lv_obj_t *parent)\n{","static void render_home(void)")
assert "lv_obj_add_event_cb(home_record_layer,on_home_scan" not in home
assert "lv_obj_remove_flag(home_record_layer,LV_OBJ_FLAG_EVENT_BUBBLE);" in home
assert "lv_obj_remove_flag(home_record_layer,LV_OBJ_FLAG_GESTURE_BUBBLE);" in home
assert "static bool recognition_touch_locked(void)" in src
assert "scan.state==LF_RECOG_CAPTURE" in section("static bool recognition_touch_locked(void)","static void swipe_intent")
assert "if(recognition_touch_locked())return;" in section("static void swipe_intent(lv_dir_t direction)","static void observe_touch_position")
assert "if(recognition_touch_locked())return;" in section("static void observe_touch_position(void)","static void finish_touch")
assert "!recognition_touch_locked()" in section("static void on_tick(lv_timer_t *timer)","static void on_clock_sweep")
assert "lv_obj_remove_flag(primary_pager,LV_OBJ_FLAG_SCROLLABLE);" in section("static void home_recording_update(","static void home_refresh(")
swipe=section("static void swipe_intent(lv_dir_t direction)","static void observe_touch_position")
assert "swipe_candidate_page = 1; /* Lyrics panel sits above Home." in swipe
assert "page == 1 && direction == LV_DIR_TOP" in swipe
scene=section("static void build_timed_lyrics_now(const lf_recog_snapshot_t *song)\n{","static void build_preview_now")
assert scene.count("lv_slider_create(timed_controls)")==1
assert scene.count("button(timed_controls,")==2
for name in ('"SLOW DOWN"','"SPEED UP"','"SAVE CURRENT"','"DEFAULT"'):
    assert name in scene,name
assert "lv_obj_set_style_bg_opa(timed_controls,LV_OPA_80,0);" in scene
assert "if(timed_scan_attempt!=song->attempts)" in scene
# Minimal lyrics has no status widget; missing labels must never stop scrolling.
assert "if(!timed_lyrics_mode||!timed_status)return;" not in src
assert "if(!timed_lyrics_mode)return;" in src
assert "if(timed_status && (force||timed_last_second!=second))" in src
fallback=section("static void build_preview_now(void)\n{","/* List rows are clickable")
assert '"NO LYRICS YET"' in fallback
assert "DEMO_TEXT" not in fallback and "SAMPLE LYRICS" not in fallback
print("PASS: recording input isolated, vertical lyrics return, simple translucent alignment")
