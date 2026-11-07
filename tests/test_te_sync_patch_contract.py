#!/usr/bin/env python3
"""0.8.127 runs tear mode NONE; the bounded GPIO-TE patch stays applied as a dormant safety net."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/"main/main.c").read_text()
version=(root/"main/version.h").read_text()
manifest=(root/"components/esp32_s3_touch_amoled_1_75/idf_component.yml").read_text()
workflow=(root/".github/workflows/firmware.yml").read_text()
patch=(root/"tools/apply_adapter_patch.py").read_text()
assert '#define LF_VERSION "0.8.127-alpha"' in version
assert "bsp_display_start_direct()" in main
assert 'version: "0.6.0"' in manifest
assert "idf.py build && python3 tools/apply_adapter_patch.py && idf.py build" in workflow
assert "pdMS_TO_TICKS(200)" in patch
assert "ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200))" in patch
assert "display_manager_flush_ready(disp);" in patch
assert "portMAX_DELAY" in patch  # source match only; replacement is bounded
assert "expected exactly one GPIO-TE infinite TX-done wait" in patch
print("PASS: tear mode NONE; bounded GPIO-TE patch retained for any future TE use")
