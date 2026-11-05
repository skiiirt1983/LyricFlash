#!/usr/bin/env python3
"""Wake-tap capture must remain alive through release; no black frame or tap-through."""
from pathlib import Path
ui=(Path(__file__).resolve().parents[1]/"main/ui.c").read_text()
wake=ui.split("static void on_wake(lv_event_t *e)\n{",1)[1].split("/* Optional hardware-level",1)[0]
finish=ui.split("static void wake_finish_async(void *arg)\n{",1)[1].split("static void on_wake",1)[0]
sleep=ui.split("static void sleep_display(void)\n{",1)[1].split("static void on_tick",1)[0]
tick=ui.split("static void on_tick_body(lv_timer_t *timer)\n{",1)[1].split("/* Append title/artist",1)[0]
assert "static bool wake_cleanup_queued;" in ui
assert "if(!asleep)return;" in wake
assert "activity();" in wake
assert "apply_brightness();" in wake
assert "lv_obj_set_style_bg_opa(sleep_overlay,LV_OPA_TRANSP,0);" in wake
assert "invalidate_full_screen();" in wake
assert "lv_async_call(wake_finish_async,NULL);" not in wake
assert "if(ui_pointer_is_down() || !sleep_overlay)return;" in finish
assert finish.index("if(ui_pointer_is_down() || !sleep_overlay)return;")<finish.index("lv_obj_delete(cover);")
assert "wake_cleanup_queued=false;" in finish
assert "invalidate_full_screen();" in finish
assert "wake_cleanup_queued=false;" in sleep
assert "lv_obj_add_event_cb(sleep_overlay,on_wake,LV_EVENT_PRESSED,NULL);" in sleep
assert "if(asleep) {" in tick
assert "if(sleep_overlay){" in tick
assert "if(!ui_pointer_is_down() && !wake_cleanup_queued){" in tick
assert "wake_cleanup_queued=true;" in tick
assert "lv_async_call(wake_finish_async,NULL);" in tick
assert tick.index("if(sleep_overlay){") < tick.index("/* Collect touch directly")
assert "lv_timer_create(on_tick, 50, NULL);" in ui
assert "bsp_display_brightness_set(asleep ? 1 : prefs->brightness)" in ui
assert "if (prefs->deep_sleep_minutes" in ui  # unchanged, opt-in 0 by default
print("PASS: wake cover stays a live touch target until release; repaint and tap-through guarded")
