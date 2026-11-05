#!/usr/bin/env python3
"""0.8.61: cancellation must never hide its own touch target or re-lock the pager.

The hardware-verified AudD/PCM worker is pinned separately by the safe-ID gate.
"""
from pathlib import Path
src=(Path(__file__).resolve().parents[1]/"main/ui.c").read_text()
def portion(a,b):
    return src.split(a,1)[1].split(b,1)[0]
callback=portion("static void on_record_cancel_double_tap(lv_event_t *e)\n{",
                 "static lv_obj_t *home_decor_ring")
tick=portion("static void on_tick(lv_timer_t *timer)\n{",
             "static void on_clock_sweep")
record=portion("static void home_recording_update(",
               "static void home_refresh(")
gesture=portion("static bool recognition_touch_locked(void)\n{",
                "static void swipe_intent")
assert "home_record_cancel_pending_attempt=scan.attempts;" in callback
assert "home_stalled_attempt=scan.attempts;" in callback
assert "atomic_store(&last_record_cancel_ms,(uint32_t)current);" in callback
assert "lf_recog_stop(); /* atomic cooperative stop; no UI mutation */" in callback
assert callback.index("home_stalled_attempt=scan.attempts;") < callback.index("lf_recog_stop();")
for unsafe in ("home_refresh(", "lv_obj_add_flag(", "lv_obj_delete(", "invalidate_full_screen("):
    assert unsafe not in callback, f"Input callback must only enqueue cancellation: {unsafe}"
assert "if(home_record_cancel_pending_attempt && !ui_pointer_is_down()" in tick
assert "const uint32_t cancelled_attempt=home_record_cancel_pending_attempt;" in tick
assert "home_record_cancel_pending_attempt=0;" in tick
assert "cancelled.attempts==cancelled_attempt" in tick
assert "if(page==0 && cancelled.attempts==cancelled_attempt){" in tick
assert "home_stalled_attempt=cancelled_attempt;" in tick
assert "lf_recog_stop(); /* cancellation flag; existing worker shuts down */" in tick
assert "lv_obj_add_flag(primary_pager,LV_OBJ_FLAG_SCROLLABLE);" in tick
assert "lv_obj_add_flag(home_record_layer,LV_OBJ_FLAG_HIDDEN);" in tick
assert "home_refresh(current);" in tick
assert "invalidate_full_screen();" in tick
assert "scan.attempts && scan.attempts==home_stalled_attempt" in gesture
assert "song->attempts!=home_stalled_attempt" in record
assert "if(home_stalled_attempt==song->attempts)" in record
assert "if(!ui_pointer_is_down() &&" in record, "Do not hide cancelled overlay before release"
assert "if(!capturing&&!working&&!opening){" in record
assert "if(!ui_pointer_is_down())" in record, "Worker completion must not hide pressed overlay"
assert "current-home_last_refresh_ms >= 750)home_refresh(current);" in tick
assert "!ui_pointer_is_down() &&\n        current-home_last_refresh_ms" in tick, "Refresh may not change recording touch target mid-press"
assert "scan.attempts!=home_stalled_attempt" in tick
assert "latest.attempts!=home_stalled_attempt" in tick
assert "song.attempts!=home_stalled_attempt" in portion(
    "static void home_refresh(uint64_t current)\n{","static void on_home_scan(")
# Display-only removal of directional microphone bias. No PCM or waveform fake sine.
assert "home_record_wave_bias_q8=0;" in record
assert "home_record_wave_bias_q8+=((signed_peak*256)-home_record_wave_bias_q8)/8;" in record
assert "const int centered=signed_peak-home_record_wave_bias_q8/256;" in record
assert "int y=130+(centered<0?swing:-swing);" in record
assert "const static int8_t phases[8]" not in src
print("PASS: immediate atomic scan stop, release-safe overlay and live microphone waveform")
