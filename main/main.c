#include "esp_log.h"
#include "nvs_flash.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "ui.h"
#include "ota_guard.h"
#include "network.h"
#include "diagnostics.h"
#include "version.h"
#include "weather.h"

static const char *TAG = "LyricFlash";

void app_main(void)
{
    // Preserve NVS on failure. NEVER auto-erase NVS: it will hold future Wi-Fi/API secrets.
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed: %s; no erase attempted", esp_err_to_name(err));
        lf_diag_error("boot", "NVS init failed");
        lf_ota_guard_failure("NVS init failed");
        return;
    }
    static lf_settings_t settings;
    err = lf_settings_load(&settings);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Settings read failed: %s", esp_err_to_name(err));
        lf_diag_error("boot", "Settings read failed");
        lf_ota_guard_failure("Settings read failed");
        return;
    }
    /* On-device AMOLED reference: TE-synchronized CO5300 QSPI panel,
       staged DMA writes and LVGL isolated from Wi-Fi on CPU 1.
       Never enable TE sync without the matched display driver. */
    bsp_display_cfg_t display_cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TE_SYNC,
        .touch_flags = { .swap_xy = 0, .mirror_x = 1, .mirror_y = 1 },
    };
    display_cfg.lv_adapter_cfg.task_core_id = 1;
    if (bsp_display_start_with_config(&display_cfg) == NULL) {
        ESP_LOGE(TAG, "Display initialization failed");
        lf_diag_error("boot", "Display start failed");
        lf_ota_guard_failure("Display start failed");
        return;
    }
    ESP_LOGI(TAG, "LyricFlash %s: demo UI and local firmware update; microphones OFF", LF_VERSION);
    if (!lf_ui_start(&settings)) {
        lf_diag_error("boot", "UI init failed");
        lf_ota_guard_failure("UI init failed");
        return;
    }
    if (!lf_network_start()) {
        ESP_LOGE(TAG,"Wi-Fi/OTA service initialization failed");
        lf_diag_error("boot", "Network init failed");
        lf_ota_guard_failure("Network recovery/update interface failed");
        return;
    }
    if (!lf_weather_start()) {
        lf_diag_error("weather", "Background weather service unavailable");
        ESP_LOGW(TAG, "Background weather service unavailable");
    }
    // Confirm only after display, settings, and local update server initialize.
    // This guards early boot failures, NOT future Wi-Fi/touch failures.
    lf_ota_guard_confirm_startup();
}
