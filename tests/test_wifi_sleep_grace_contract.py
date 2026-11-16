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
# 1.1.1: the radio is never stopped for screen-off; modem sleep keeps the web
# page reachable, with full speed for a minute after any web request.
assert "wifi_idle_worker" not in net and "Idle Wi-Fi radio suspended" not in net
assert "static void wifi_power_apply(void)" in net and "if (in_ap) return;" in net
assert "!atomic_load(&ota_busy))" in net and "WIFI_PS_MAX_MODEM : (int)WIFI_PS_MIN_MODEM" in net
assert "#define LF_WEB_ACTIVE_MS 60000U" in net and "cfg.open_fn=on_http_open;" in net
assert "esp_wifi_connect();" in net.split("WIFI_EVENT_STA_DISCONNECTED",1)[1].split("IP_EVENT_STA_GOT_IP",1)[0]
assert "lf_network_wifi_power_state(void)" in net
assert "ui_label_set_text_if_changed(battery_status,value);" in ui
assert "ui_label_set_text_if_changed(network_password, text);" in ui
assert "on_tick_body(timer);" in ui
assert "ui_tick_work_last_us" in net and "pager_frame_gap_last_ms" in net
print("PASS: 60s default, 120s radio grace, guarded reconnect and profiled UI timing")
