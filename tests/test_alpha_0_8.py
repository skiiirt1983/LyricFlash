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
assert 'lv_obj_set_style_translate_y(' not in ui
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
assert 'button(parent, "CONNECT / OTA"' in ui
assert 'static void build_connection_help(void)' in ui
assert 'lv_async_call(commit_swipe, NULL)' in ui
assert 'network_status=label(parent, "Wi-Fi starting..."' in ui
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
assert '0.8.38-alpha' in (p/'main/version.h').read_text()
assert 'set(PROJECT_VER "0.8.38-alpha")' in (p/'CMakeLists.txt').read_text()
assert 'ESP-IDF image descriptor version' in (p/'tools/check_build.py').read_text()
assert 'LyricFlash-Alpha-0.8.38-FULL-USB-and-OTA-UNTESTED' in workflow
print('PASS: Alpha 0.8 diagnostics, PIN-free Web UI and navigation gates')

assert 'clock_face' in net and 'weather_enabled' in net
assert 'clock_face' in web and 'weather_enabled' in web
assert 'prefs->clock_face = basic->clock_face' in ui
