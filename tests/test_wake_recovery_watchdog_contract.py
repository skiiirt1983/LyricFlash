#!/usr/bin/env python3
"""0.8.90 safe post-wake stall restart, persistent breadcrumb, no OTA/sleep regressions."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ui=(root/"main/ui.c").read_text()
head=(root/"main/ui.h").read_text()
app=(root/"main/main.c").read_text()
net=(root/"main/network.c").read_text()
net_h=(root/"main/network.h").read_text()
diag=(root/"main/diagnostics.c").read_text()
diag_h=(root/"main/diagnostics.h").read_text()
assert "static atomic_bool ui_screen_awake;" in ui
assert "static atomic_bool wake_overlay_pending;" in ui
assert "out->screen_awake=atomic_load(&ui_screen_awake);" in ui
assert "out->wake_overlay_pending=atomic_load(&wake_overlay_pending);" in ui
assert "out->last_wake_age_ms=wake_at?current-wake_at:UINT32_MAX;" in ui
for f in ("screen_awake","wake_overlay_pending","last_wake_age_ms"): assert f in head
wake=ui.split("static void on_wake(lv_event_t *e)",1)[1].split("/* Optional hardware-level",1)[0]
assert wake.index("atomic_store(&ui_screen_awake,true);")<wake.index("lf_power_set_screen_awake(true);")
assert wake.index("lf_power_set_screen_awake(true);")<wake.index("apply_brightness();")
assert wake.index("apply_brightness();")<wake.index("lf_network_set_idle(false);")
assert "atomic_store(&wake_overlay_pending,true);" in wake
finish=ui.split("static void wake_finish_async(void *arg)",1)[1].split("static void on_wake(",1)[0]
assert finish.index("lv_obj_delete(cover);")<finish.index("atomic_store(&wake_overlay_pending,false);")
sleep=ui.split("static void sleep_display(void)",1)[1].split("static void on_tick",1)[0]
assert "atomic_store(&ui_screen_awake,false);" in sleep
assert "atomic_store(&wake_overlay_pending,false);" in sleep
assert "wake_window=ui.screen_awake &&" in app
assert "ui.last_wake_age_ms<120000U" in app
assert "ui.ui_tick_age_ms>25000U" in app
assert "ui.wake_overlay_pending && ui.last_wake_age_ms>45000U" in app
assert "lf_network_ota_in_progress()" in app and "lf_network_ota_in_progress(void)" in net
assert "lf_network_ota_in_progress(void);" in net_h
assert "lf_diag_wake_recovery(reason,ui.last_wake_age_ms,ui.ui_tick_age_ms);" in app
assert "lf_diag_wake_recovery(uint8_t reason,uint32_t age_ms,uint32_t tick_age_ms)" in diag
assert "nvs_open(" + chr(34) + "lf_wake_dbg" + chr(34) in diag
assert "nvs_commit(h)" in diag
assert "nvs_flash_erase(" not in app and "nvs_flash_erase(" not in diag
assert "wake_recovery_count" in diag_h and "wake_recovery_last_reason" in diag_h
for k in ("ui_screen_awake","ui_wake_overlay_pending","ui_last_wake_age_ms","wake_recovery_count","wake_recovery_last_reason_code","wake_recovery_last_age_ms","wake_recovery_last_tick_age_ms"):
    assert chr(34)+k+chr(34) in net
assert "LF_PAGER_STALL_GUARD_MS 1600U" in ui and "lv_obj_scroll_to_x(primary_pager,target*466,LV_ANIM_ON);" in ui
assert "(uint32_t)current-off_at>=120000U" in ui
print("PASS: wake-first CPU/UI, OTA-safe scoped watchdog, NVS-only-on-stall breadcrumbs, preserved pager/grace")
