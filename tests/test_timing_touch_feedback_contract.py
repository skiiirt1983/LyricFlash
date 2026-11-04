#!/usr/bin/env python3
"""0.8.61 small, reliable controls and the quiet cancel completion path."""
from pathlib import Path
s=(Path(__file__).resolve().parents[1]/"main/ui.c").read_text()
def section(begin,end):
    return s.split(begin,1)[1].split(end,1)[0]
cancel=section("if(home_record_cancel_pending_attempt && !ui_pointer_is_down()",
               "/* Never start a deferred microphone capture during touch")
nudge=section("static void timed_nudge_feedback_end(lv_timer_t *timer)",
              "static void timed_load_default(lv_event_t *e)")
scene=section("static void build_timed_lyrics_now(const lf_recog_snapshot_t *song)",
              "static void build_preview_now(void)")
close=section("static void lyrics_overlay_close(void)","static void make_base(void)")
reset=section("static void make_base(void)","static void ")
assert 'home_warning="Cancelling scan...";' not in cancel
assert "home_warning=NULL;" in cancel and "home_warning_until_ms=0;" in cancel
assert "lf_recog_stop(); /* cancellation flag; existing worker shuts down */" in cancel
assert "home_refresh(current);" in cancel
assert "lv_obj_set_style_bg_opa(timed_controls,(lv_opa_t)38,0);" in scene
assert "lv_obj_set_size(control,72,64);" in nudge
assert "lv_obj_align(control,LV_ALIGN_CENTER,x,-112);" in nudge
assert "LV_STATE_PRESSED" in nudge and "LV_STATE_USER_1" in nudge
assert "lv_obj_set_style_shadow_width(control,14,LV_STATE_PRESSED);" in nudge
assert "lv_obj_set_style_shadow_width(control,12,LV_STATE_USER_1);" in nudge
assert "timed_nudge_feedback_timer=lv_timer_create(timed_nudge_feedback_end,280,NULL);" in nudge
assert "lv_timer_reset(timed_nudge_feedback_timer);" in nudge
assert "lv_timer_resume(timed_nudge_feedback_timer);" in nudge
assert "lv_timer_pause(timer);" in nudge
assert "lv_obj_add_state(control,LV_STATE_USER_1);" in nudge
assert "timed_nudge_feedback(lv_event_get_current_target(e));" in nudge
assert "lv_timer_delete(timed_nudge_feedback_timer);" in close
assert "lv_timer_delete(timed_nudge_feedback_timer);" in reset
assert "timed_nudge_feedback_button=NULL;" in close
assert "timed_nudge_feedback_button=NULL;" in reset
assert 'button(timed_controls,"RESET"' in scene
assert '"SAVE CURRENT"' not in scene
print("PASS: no lingering cancel label, ~15% overlay, larger nudge targets and bounded glow lifetime")
