#!/usr/bin/env python3
"""Non-hardware alpha safety rules. Not a replacement for full ESP-IDF compilation."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
main=(p/'main/main.c').read_text()
ui=(p/'main/ui.c').read_text()
settings=(p/'main/settings.c').read_text()
partitions=(p/'partitions.csv').read_text()
manifest=(p/'main/idf_component.yml').read_text()
assert 'nvs_flash_erase' not in main and 'nvs_flash_erase' not in settings
assert 'lf_ota_guard_start_health_check();' in main
assert 'lf_ota_guard_confirm_startup();' not in main
assert main.index('lf_network_start()') < main.index('lf_ota_guard_start_health_check();')
assert 'bsp_display_start_with_config(&display_cfg)' in main
assert 'bsp_display_brightness_set' in ui
assert 'nvs_commit(h)' in settings
assert '0x500000' in partitions and '0x100000' in partitions
assert 'waveshare/esp32_s3_touch_amoled_1_75' not in manifest
assert 'esp32_s3_touch_amoled_1_75' in (p/'main/CMakeLists.txt').read_text()
assert (p/'components/esp32_s3_touch_amoled_1_75/LICENSE').exists()
assert '"9.5.0"' in manifest
assert 'microphone' in main.lower() and 'OFF' in main
print('PASS: static alpha safety gates and board dependency metadata')
