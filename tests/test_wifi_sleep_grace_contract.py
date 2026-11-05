#!/usr/bin/env python3
"""Guard 0.8.84 Wi-Fi grace path and nonblocking UI profiling."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ui=(root/"main/ui.c").read_text()
net=(root/"main/network.c").read_text()
prefs=(root/"main/settings.c").read_text()
body=ui.split("static void on_tick_body(lv_timer_t *timer)\n{",1)[1].split("/* Measures the entire 50ms UI callback",1)[0]
sleep=body.split("    if(asleep) {\n",1)[1].split("    /* After waking, consume",1)[0]
assert "(uint32_t)current-off_at>=120000U" in sleep
assert "lf_network_set_idle(true);" in sleep
assert sleep.index("lf_network_set_idle(true);") < sleep.index("        return;")
assert body.count("lf_network_set_idle(true);")==1
assert "lf_network_set_idle(false);" in ui.split("static void on_wake",1)[1]
assert "last_wifi_resume_retry_ms" in body
assert ".sleep_seconds = 60," in prefs
assert "if (idle && (in_ap || atomic_load(&ota_busy))) return;" in net
assert "if (in_ap || atomic_load(&ota_busy)) break;" in net
assert "esp_wifi_stop()" in net and "esp_wifi_start()" in net
assert "wifi_retry_after_ms" in net and "+1000U" in net
assert "lf_network_wifi_power_state(void)" in net
assert "ui_label_set_text_if_changed(battery_status,value);" in ui
assert "ui_label_set_text_if_changed(network_password, text);" in ui
assert "on_tick_body(timer);" in ui
assert "ui_tick_work_last_us" in net and "pager_frame_gap_last_ms" in net
print("PASS: 60s default, 120s radio grace, guarded reconnect and profiled UI timing")
