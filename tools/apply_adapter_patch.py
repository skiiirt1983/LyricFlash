#!/usr/bin/env python3
"""Apply a bounded GPIO-TE TX-done wait to esp_lvgl_adapter 0.6.0."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
adapter = root / "managed_components/espressif__esp_lvgl_adapter/src/display/bridge/v9/lvgl_bridge_v9.c"
if not adapter.is_file():
    raise SystemExit(f"ERROR: expected esp_lvgl_adapter source not found: {adapter}")

source = adapter.read_text(encoding="utf-8")
marker = "LyricFlash bounded TE TX-done wait (200ms)"
if marker in source:
    print("PASS: bounded GPIO-TE TX-done wait already applied")
    raise SystemExit(0)

signature = "static void display_bridge_v9_flush_gpio_te"
first = source.find(signature)
if first < 0:
    raise SystemExit("ERROR: GPIO-TE flush function not found")
start = source.find(signature, first + len(signature))
if start < 0:
    raise SystemExit("ERROR: GPIO-TE flush function definition not found")
end = source.find("static void display_bridge_v9_flush_double_full", start)
if end < 0:
    raise SystemExit("ERROR: end of GPIO-TE flush function not found")

function = source[start:end]
pattern = re.compile(
    r"/\* Wait for transmission to complete \*/\s*"
    r"ulTaskNotifyTake\(pdTRUE,\s*portMAX_DELAY\);\s*"
    r"(?:if\s*\(impl->cfg.te_ctx\)\s*\{\s*"
    r"esp_lv_adapter_te_sync_record_tx_done\(impl->cfg.te_ctx\);\s*\}\s*)?"
    r"display_manager_flush_ready\(disp\);"
)
replacement = """/* LyricFlash bounded TE TX-done wait (200ms). */
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200)) == 0) {
        ESP_LOGW(TAG, "GPIO TE flush TX-done notify timed out (200ms) -- releasing LVGL flush");
    }
    if (impl->cfg.te_ctx) {
        esp_lv_adapter_te_sync_record_tx_done(impl->cfg.te_ctx);
    }
    display_manager_flush_ready(disp);"""
function, count = pattern.subn(replacement, function, count=1)
if count != 1:
    raise SystemExit("ERROR: expected exactly one GPIO-TE infinite TX-done wait; refusing an unverified patch")
source = source[:start] + function + source[end:]
adapter.write_text(source, encoding="utf-8")
print("PASS: GPIO-TE TX-done wait bounded to 200 ms; TE state recorded on completion/timeout")
