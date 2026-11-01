#include "ota_guard.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

static const char *TAG = "LyricFlashOTA";

static bool pending_verification(void)
{
    const esp_partition_t *partition = esp_ota_get_running_partition();
    if (partition == NULL) return false;
    esp_ota_img_states_t state;
    return esp_ota_get_state_partition(partition, &state) == ESP_OK &&
           state == ESP_OTA_IMG_PENDING_VERIFY;
}

void lf_ota_guard_failure(const char *reason)
{
    ESP_LOGE(TAG, "Startup failed: %s", reason);
    if (pending_verification()) {
        ESP_LOGE(TAG, "Rejecting unconfirmed OTA image; requesting rollback");
        esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
        ESP_LOGE(TAG, "Rollback unavailable: %s", esp_err_to_name(err));
    } else {
        ESP_LOGW(TAG, "No pending OTA verification; manual USB recovery may be needed");
    }
}

void lf_ota_guard_confirm_startup(void)
{
    if (pending_verification()) {
        esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
        if (err != ESP_OK) ESP_LOGE(TAG, "Cannot confirm boot: %s", esp_err_to_name(err));
        else ESP_LOGI(TAG, "Minimal startup passed; OTA image confirmed");
    } else {
        ESP_LOGW(TAG, "No pending OTA verification (prior boot or bootloader rollback disabled)");
    }
}
