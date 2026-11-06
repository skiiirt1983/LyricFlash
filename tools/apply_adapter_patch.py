#!/usr/bin/env python3
"""Apply the bounded GPIO-TE completion guard to esp_lvgl_adapter 0.6.0.

CI first resolves managed components, applies this guarded patch, then rebuilds.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
adapter = root / "managed_components/espressif__esp_lvgl_adapter/src/display/bridge/v9/lvgl_bridge_v9.c"
if not adapter.is_file():
    raise SystemExit(f"ERROR: expected esp_lvgl_adapter source not found: {adapter}")
source = adapter.read_text(encoding="utf-8")
helper_marker = "LyricFlash local patch: %s TX-done notify timed out"
helper_find = """    return row_bytes;
}

/**
 * @brief Default flush (single buffer, no tear protection)
 */
"""
helper_replace = """    return row_bytes;
}

static bool display_bridge_v9_wait_tx_done(esp_lv_adapter_display_bridge_v9_t *impl,
                                           const char *context)
{
    const TickType_t timeout = pdMS_TO_TICKS(200);
    if (ulTaskNotifyTake(pdTRUE, timeout) != 0) {
        return true;
    }
    ESP_LOGW(TAG, "LyricFlash local patch: %s TX-done notify timed out (%ums) -- releasing LVGL flush",
             context ? context : "display", (unsigned)pdTICKS_TO_MS(timeout));
    if (impl && impl->cfg.te_ctx) {
        esp_lv_adapter_te_sync_record_tx_done(impl->cfg.te_ctx);
    }
    return false;
}

/**
 * @brief Default flush (single buffer, no tear protection)
 */
"""
if helper_marker not in source:
    if helper_find not in source:
        raise SystemExit("ERROR: adapter helper insertion point not found; refusing an unverified patch")
    source = source.replace(helper_find, helper_replace, 1)
gpio_marker = 'display_bridge_v9_wait_tx_done(impl, "GPIO TE flush")'
gpio_find = """    /* Wait for transmission to complete */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (impl->cfg.te_ctx) {
        esp_lv_adapter_te_sync_record_tx_done(impl->cfg.te_ctx);
    }
    display_manager_flush_ready(disp);
"""
gpio_replace = """    /* Bound the panel-I/O completion wait so a missed notification cannot
     * hold the LVGL worker and its mutex indefinitely. */
    const bool tx_done = display_bridge_v9_wait_tx_done(impl, "GPIO TE flush");
    if (tx_done && impl->cfg.te_ctx) {
        esp_lv_adapter_te_sync_record_tx_done(impl->cfg.te_ctx);
    }
    display_manager_flush_ready(disp);
"""
if gpio_marker not in source:
    if gpio_find not in source:
        raise SystemExit("ERROR: adapter GPIO-TE wait block not found; refusing an unverified patch")
    source = source.replace(gpio_find, gpio_replace, 1)
adapter.write_text(source, encoding="utf-8")
print("PASS: esp_lvgl_adapter GPIO-TE TX-done wait bounded to 200 ms")
