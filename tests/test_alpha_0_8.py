#!/usr/bin/env python3
"""Alpha 0.8 hardware-independent feature gates."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
ui=(p/'main/ui.c').read_text()
wheel=(p/'main/char_wheel.c').read_text()
net=(p/'main/network.c').read_text()
web=(p/'main/web_basic_html.h').read_text()
diag=(p/'main/diagnostics.c').read_text()
workflow=(p/'.github/workflows/pages-and-firmware.yml').read_text()
assert 'lv_screen_load_anim(' not in ui
assert 'nav_settle(' not in ui
assert 'pager_build(' not in ui
assert 'lv_obj_set_style_translate_x(' not in ui
# Pager transitions must not use CSS translation; the dedicated lyric
# layer is the sole exception, and never moves the Clock/Home/Settings pager.
assert ui.count('lv_obj_set_style_translate_y(')==1
assert 'lv_obj_set_style_translate_y(timed_scroll_group,target,0)' in ui
assert 'lv_anim_set_exec_cb(' not in ui
assert 'render_home();' in ui and 'render_clock();' in ui and 'render_settings();' in ui
assert 'lv_indev_get_state(ptr)==LV_INDEV_STATE_PRESSED' in ui
assert 'lv_screen_load(screen);' in ui
assert 'lv_obj_delete_async(old_screen);' in ui
assert 'LF_SWIPE_MIN_PX 38' in ui
assert 'LF_CLOCK_SETTINGS_HOLD_MS 2000ULL' in ui
assert 'show_clock_settings_popup' in ui
assert 'lv_obj_add_event_cb(screen,on_lvgl_gesture,LV_EVENT_GESTURE,NULL)' in ui
assert 'lv_obj_add_event_cb(b, on_lvgl_gesture, LV_EVENT_GESTURE, NULL)' in ui
assert 'LV_EVENT_PRESS_LOST means the finger left the original widget' in ui
assert 'lv_indev_get_state(gesture_indev) == LV_INDEV_STATE_RELEASED' in ui
assert 'static void finish_touch(void)' in ui
assert 'swipe_intent(lv_indev_get_gesture_dir(indev))' in ui
assert 'Do NOT clear gesture state' in ui
assert 'swipe_candidate_page' in ui
# The actual display used to trap users on Settings (page 2).
# Require a deliberate swipe for both directions and an exit path.
assert 'else if (page == 2)' in ui
settings_swipe=ui.split('else if (page == 2)',1)[1].split('else if (page == 6)',1)[0]
assert 'LV_DIR_LEFT || direction == LV_DIR_RIGHT' in settings_swipe
assert 'swipe_candidate_page = 0;' in settings_swipe
assert 'lv_indev_get_state(gesture_indev) == LV_INDEV_STATE_RELEASED' in ui
assert 'touch_held' in ui and 'on_touch_release' in ui
assert 'lv_screen_load(screen);' in ui
assert 'lv_obj_delete_async(old_screen);' in ui
assert 'home_scan_button=lv_button_create(parent);' in ui
assert 'lv_obj_add_event_cb(home_scan_button,on_home_scan,LV_EVENT_CLICKED,NULL)' in ui
assert 'static void build_connection_help(void)' in ui
assert 'lv_async_call(commit_swipe, NULL)' in ui
assert 'battery_status=home_caption(parent' in ui
assert 'lv_obj_delete(old_screen);' not in ui
assert 'build_debug();' in ui and 'page == 14' in ui
assert 'lf_diag_snapshot(&d);' in ui and 'lf_ui_max_tick_gap_ms' in ui
assert 'add_space(' in wheel and 'SPACE' in wheel and "input[n]=' '" in wheel
for endpoint in ['/settings','/api/basic','/api/diagnostics']:
    assert endpoint in net, endpoint
assert 'pin_matches' not in net and 'X-LyricFlash-PIN' not in net
assert 'lf_network_arm_ota' not in ui and 'lf_network_ota_pin' not in ui
assert 'config_pin' not in web
assert 'handle_github_install' in net and 'handle_firmware_page' in net
assert 'lf_ui_apply_basic_settings' in net
assert 'lf_weather_save_config(&weather)' in net
assert 'httpd_resp_set_hdr(req, "Cache-Control", "no-store")' in net
for key in ['font_style','font_size','ring_mode','show_date','weather','background','foreground','ring_a','ring_b','ring_c','city','interval_minutes']:
    assert key in web and key in net, key
assert 'microphone' in net and 'recent_errors' in net
assert 'uxTaskGetNumberOfTasks' in diag
# The project metadata must match the device's reported firmware version.
# A stale ESP-IDF PROJECT_VER produces a mismatched application descriptor,
# which must fail before packaging or offering any OTA installation.
import re
header=(p/'main/version.h').read_text()
match=re.search(r'^#define LF_VERSION "([^"]+)"',header,re.MULTILINE)
assert match, "Missing LF_VERSION"
version=match.group(1)
assert version in ("0.8.41-alpha","0.8.42-alpha","0.8.43-alpha","0.8.44-alpha","0.8.45-alpha","0.8.46-alpha","0.8.47-alpha","0.8.48-alpha","0.8.49-alpha","0.8.50-alpha","0.8.51-alpha","0.8.52-alpha","0.8.53-alpha","0.8.59-alpha"), version
assert f'set(PROJECT_VER "{version}")' in (p/'CMakeLists.txt').read_text(), (
    "ESP-IDF app descriptor PROJECT_VER must agree with LF_VERSION"
)
assert 'ESP-IDF image descriptor version' in (p/'tools/check_build.py').read_text()
assert 'LyricFlash-Alpha-0.8.41-FULL-USB-and-OTA-UNTESTED' in workflow
print('PASS: Alpha 0.8 diagnostics, PIN-free Web UI and navigation gates')

assert 'clock_face' in net and 'weather_enabled' in net
assert 'clock_face' in web and 'weather_enabled' in web
assert 'prefs->clock_face = basic->clock_face' in ui
