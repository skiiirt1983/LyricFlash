#!/usr/bin/env python3
"""0.8.82: power savings only when screen dark, without changing the wake/OTA path."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=(root/"main/power.c").read_text()
h=(root/"main/power.h").read_text()
ui=(root/"main/ui.c").read_text()
main=(root/"main/main.c").read_text()
net=(root/"main/network.c").read_text()
sdk=(root/"sdkconfig.defaults").read_text()
cmake=(root/"main/CMakeLists.txt").read_text()
def section(src,a,b):
    assert a in src and b in src
    return src.split(a,1)[1].split(b,1)[0]
sleep=section(ui,"static void sleep_display(void)\n{","static void on_tick")
wake=section(ui,"static void on_wake(lv_event_t *e)\n{","/* Optional hardware-level")
assert "CONFIG_PM_ENABLE=y" in sdk
assert "CONFIG_PM_DFS_INIT_AUTO=n" in sdk
assert "CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y" in sdk
assert "        esp_pm\n" in cmake
assert '#include "esp_pm.h"' in p
assert ".max_freq_mhz=240" in p and ".min_freq_mhz=80" in p
assert ".light_sleep_enable=false" in p
assert "ESP_PM_CPU_FREQ_MAX" in p
assert "esp_pm_lock_acquire(screen_cpu_lock)" in p
assert "esp_pm_lock_release(screen_cpu_lock)" in p
assert "profile.min_freq_mhz=240;" in p
assert "static atomic_bool screen_cpu_lock_held;" in p
assert "atomic_load(&screen_cpu_lock_held)" in p
assert "atomic_store(&screen_cpu_lock_held,awake);" in p
assert "lf_power_cpu_profile_start()" in main
assert main.index("lf_power_cpu_profile_start()")<main.index("bsp_display_start_direct")
assert "lf_power_set_screen_awake(true);" in wake
assert wake.index("lf_power_set_screen_awake(true);")<wake.index("apply_brightness();")
assert "lf_power_set_screen_awake(false);" in sleep
assert sleep.index("lv_obj_add_event_cb(sleep_overlay,on_wake,LV_EVENT_PRESSED,NULL);")<sleep.index("lf_power_set_screen_awake(false);")
assert sleep.index("apply_brightness();")<sleep.index("lf_power_set_screen_awake(false);")
assert "lv_timer_pause(clock_sweep_timer);" in sleep
assert "lv_timer_resume(clock_sweep_timer);" in ui
assert 'cJSON_AddStringToObject(root, "cpu_power_profile", lf_power_cpu_profile_state());' in net
assert "lf_power_cpu_profile_state(void);" in h
assert 'return "fixed_240";' in p and '"awake_240":"screen_off_dfs_80"' in p
assert "esp_wifi_set_ps(" not in net
assert "nvs_flash_erase" not in main
assert "esp_deep_sleep_start();" in ui
assert ".light_sleep_enable=true" not in p
print("PASS: 240 MHz awake, screen-dark CPU DFS floor 80 MHz, live touch + Wi-Fi + OTA, fail-safe fixed-speed fallback")
