#!/usr/bin/env python3
"""Screen-off power stage 1: suppress unused 16ms clock ticks; never delay touch wake."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ui=(root/"main/ui.c").read_text()
main=(root/"main/main.c").read_text()
network=(root/"main/network.c").read_text()
power=(root/"main/power.c").read_text()

sleep=ui.split("static void sleep_display(void)\n{",1)[1].split("static void on_tick",1)[0]
wake=ui.split("static void wake_finish_async(void *arg)\n{",1)[1].split("static void on_wake",1)[0]
sweep=ui.split("static void on_clock_sweep(lv_timer_t *timer)\n{",1)[1].split("\n/* Deliberately NOT",1)[0]
assert "static lv_timer_t *clock_sweep_timer;" in ui
assert "clock_sweep_timer=lv_timer_create(on_clock_sweep, 16, NULL);" in ui
assert "if(clock_sweep_timer)lv_timer_pause(clock_sweep_timer);" in sleep
assert sleep.index("asleep=true;") < sleep.index("lv_timer_pause(clock_sweep_timer)") < sleep.index("apply_brightness();")
assert "lv_obj_add_event_cb(sleep_overlay,on_wake,LV_EVENT_PRESSED,NULL);" in sleep
assert "lv_timer_reset(clock_sweep_timer);" in wake
assert "lv_timer_resume(clock_sweep_timer);" in wake
assert wake.index("if(cover)lv_obj_delete(cover);") < wake.index("invalidate_full_screen();") < wake.index("lv_timer_resume(clock_sweep_timer);")
assert "if(!asleep)return;" in ui.split("static void on_wake(lv_event_t *e)\n{",1)[1].split("\n/* Optional hardware-level",1)[0]
assert "lv_timer_create(on_tick, 50, NULL);" in ui
assert "lv_indev_get_state(wake)==LV_INDEV_STATE_PRESSED" in ui
assert "page == 6" in sweep and "!asleep" in sweep
assert "bsp_display_brightness_set(asleep ? 1 : prefs->brightness)" in ui
assert "esp_wifi_set_ps(" not in network
assert "lf_power_read(&sample)" in power
assert "lf_ota_guard_start_health_check()" in main
print("PASS: 16ms clock animation is suspended only during OLED screen-off; touch, Wi-Fi, OTA preserved")
