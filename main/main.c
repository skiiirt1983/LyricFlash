#include "esp_log.h"
#include "nvs_flash.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "ui.h"
#include "ota_guard.h"
#include "network.h"

static const char *TAG = "LyricFlash";

void app_main(void)
{
    // Preserve NVS on failure. NEVER auto-erase NVS: it will hold future Wi-Fi/API secrets.
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed: %s; no erase attempted", esp_err_to_name(err));
        lf_ota_guard_failure("NVS init failed");
        return;
    }
    static lf_settings_t settings;
    err = lf_settings_load(&settings);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Settings read failed: %s", esp_err_to_name(err));
        lf_ota_guard_failure("Settings read failed");
        return;
    }
    if (bsp_display_start() == NULL) {
        ESP_LOGE(TAG, "Display initialization failed");
        lf_ota_guard_failure("Display start failed");
        return;
    }
    ESP_LOGI(TAG, "LyricFlash alpha 0.4.0: demo UI and local firmware update; microphones OFF");
    if (!lf_ui_start(&settings)) {
        lf_ota_guard_failure("UI init failed");
        return;
    }
    if (!lf_network_start()) {
        ESP_LOGE(TAG,"Wi-Fi/OTA service initialization failed");
        lf_ota_guard_failure("Network recovery/update interface failed");
        return;
    }
    // Confirm only after display, settings, and local update server initialize.
    // This guards early boot failures, NOT future Wi-Fi/touch failures.
    lf_ota_guard_confirm_startup();
}
