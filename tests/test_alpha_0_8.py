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
assert 'lv_screen_load(screen);' in ui
assert 'lv_obj_delete(old_screen);' in ui
assert 'build_debug();' in ui and 'page == 14' in ui
assert 'lf_diag_snapshot(&d);' in ui and 'lf_ui_max_tick_gap_ms' in ui
assert 'add_space(' in wheel and 'SPACE' in wheel and "input[n]=' '" in wheel
for endpoint in ['/settings','/api/basic','/api/diagnostics']:
    assert endpoint in net, endpoint
assert 'if (!pin_matches(req))' in net
assert 'lf_ui_apply_basic_settings' in net
assert 'lf_weather_save_config(&weather)' in net
assert 'httpd_resp_set_hdr(req, "Cache-Control", "no-store")' in net
for key in ['font_style','font_size','ring_mode','show_date','weather','background','foreground','ring_a','ring_b','ring_c','city','interval_minutes']:
    assert key in web and key in net, key
assert 'microphone' in net and 'recent_errors' in net
assert 'uxTaskGetNumberOfTasks' in diag
assert '0.8.0-alpha' in (p/'main/version.h').read_text()
assert 'LyricFlash-Alpha-0.8-FULL-USB-and-OTA-UNTESTED' in workflow
print('PASS: Alpha 0.8 diagnostics, local settings auth and navigation gates')
