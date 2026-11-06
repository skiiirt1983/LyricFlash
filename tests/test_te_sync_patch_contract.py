#!/usr/bin/env python3
"""Protect the 0.8.100 TE-sync configuration and adapter timeout patch."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/"main/main.c").read_text()
version=(root/"main/version.h").read_text()
manifest=(root/"components/esp32_s3_touch_amoled_1_75/idf_component.yml").read_text()
workflow=(root/".github/workflows/firmware.yml").read_text()
patch=(root/"tools/apply_adapter_patch.py").read_text()
assert '#define LF_VERSION "0.8.100-alpha"' in version
assert ".tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TE_SYNC" in main
assert 'version: "0.6.0"' in manifest
assert "idf.py build && python3 tools/apply_adapter_patch.py && idf.py build" in workflow
assert "pdMS_TO_TICKS(200)" in patch
assert "ulTaskNotifyTake(pdTRUE, timeout)" in patch
assert "display_manager_flush_ready(disp);" in patch
replacement=patch.split("gpio_replace =",1)[1].split("if gpio_marker",1)[0]
assert "portMAX_DELAY" not in replacement
assert "refusing an unverified patch" in patch
print("PASS: TE_SYNC is enabled only with adapter 0.6.0 and bounded TX-done recovery")
