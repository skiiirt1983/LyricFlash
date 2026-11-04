#!/usr/bin/env python3
"""0.8.73: nested Device Settings touch cannot trigger timeout or wake black."""
from pathlib import Path
ui=(Path(__file__).resolve().parents[1]/"main/ui.c").read_text()
tick=ui.split("static void on_tick(lv_timer_t *timer)\n{",1)[1].split("/* Only the active Rolex-style",1)[0]
wake=ui.split("static void on_wake(lv_event_t *e)\n{",1)[1].split("/* Optional hardware-level",1)[0]
sleep=ui.split("static void sleep_display(void)\n{",1)[1].split("static void on_tick",1)[0]
cleanup=ui.split("static void wake_finish_async(void *arg)\n{",1)[1].split("static void on_wake",1)[0]
settings=ui.split("static void build_device_settings_now(void)\n{",1)[1].split("/* Clock dial elements",1)[0]
assert "page=10;" in settings and "lv_obj_set_scroll_dir(viewport,LV_DIR_VER);" in settings
assert "lv_obj_remove_flag(slider,LV_OBJ_FLAG_EVENT_BUBBLE);" in settings
assert "if(!asleep && ui_pointer_is_down())last_activity_ms=current;" in tick
assert tick.index("if(!asleep && ui_pointer_is_down())last_activity_ms=current;")<tick.index("if (nav_transition_active) return;")
assert "if(asleep || ui_pointer_is_down() || pager_scrolling ||" in sleep
assert "!ui_pointer_is_down() &&" in tick and "current - last_activity_ms >" in tick
assert "lv_obj_set_style_bg_opa(sleep_overlay,LV_OPA_TRANSP,0);" in wake
assert "lv_obj_set_style_bg_opa(sleep_overlay,LV_OPA_TRANSP,LV_STATE_PRESSED);" in wake
assert "lv_async_call(wake_finish_async,NULL);" not in wake
assert "if(ui_pointer_is_down() || !sleep_overlay)return;" in cleanup
assert "lv_obj_delete(cover);" in cleanup
assert "lv_timer_resume(clock_sweep_timer);" in cleanup
assert "lv_timer_create(on_tick, 50, NULL);" in ui
assert "if (prefs->deep_sleep_minutes" in tick  # 0 by default, not activated by fix
print("PASS: nested settings touch refreshes inactivity; held finger never sleeps; press/release cover styles match")
