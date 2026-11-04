#include "ota_guard.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui.h"
#include "diagnostics.h"
#include "ota_health_logic.h"

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

/* OTA rollback contract: for a NEW, pending slot only, verify that the UI
   continues to tick and actually completes refreshes after network startup.
   Do not test Internet connectivity, touch presses or recognition -- those
   are optional user actions, not requirements for a healthy boot. */
#define LF_OTA_HEALTH_SAMPLE_MS 2500U
#define LF_OTA_HEALTH_DEADLINE_MS 40000U
#define LF_OTA_HEALTH_REQUIRED_SAMPLES 3U

static void ota_health_task(void *unused)
{
    (void)unused;
    const int64_t started_ms=esp_timer_get_time()/1000LL;
    unsigned stable_samples=0;
    do {
        vTaskDelay(pdMS_TO_TICKS(LF_OTA_HEALTH_SAMPLE_MS));
        if (!pending_verification()) break; /* Already handled elsewhere. */
        lf_ui_display_stats_t ui={0};
        lf_ui_display_stats(&ui); /* Lock-free atomics; never take LVGL lock. */
        const lf_ota_health_sample_t sample={
            .ui_tick_age_ms=ui.ui_tick_age_ms,
            .render_count=ui.render_count,
            .flush_finishes=ui.flush_finishes,
            .refresh_ready=ui.refresh_ready,
            .free_heap_bytes=esp_get_free_heap_size()
        };
        if (lf_ota_health_sample_ok(&sample)) {
            stable_samples++;
            if (stable_samples>=LF_OTA_HEALTH_REQUIRED_SAMPLES) {
                ESP_LOGI(TAG,"OTA health verified: LVGL heartbeat, rendered frames and heap");
                lf_ota_guard_confirm_startup();
                vTaskDelete(NULL);
                return;
            }
        } else {
            stable_samples=0; /* Never pass on disconnected good samples. */
        }
    } while ((esp_timer_get_time()/1000LL-started_ms)<LF_OTA_HEALTH_DEADLINE_MS);

    if (pending_verification()) {
        ESP_LOGE(TAG,"New OTA image did not sustain LVGL UI health; retaining prior image");
        lf_diag_error("ota", "Post-boot UI health check failed");
        lf_ota_guard_failure("Post-boot UI health check failed");
    }
    vTaskDelete(NULL);
}

bool lf_ota_guard_start_health_check(void)
{
    if (!pending_verification()) return true; /* No task on known-good boots. */
    if (xTaskCreate(ota_health_task,"lf_ota_health",3584,NULL,
                    tskIDLE_PRIORITY+1,NULL)!=pdPASS) {
        lf_diag_error("ota","Cannot launch post-boot health verifier");
        lf_ota_guard_failure("Cannot launch post-boot health verifier");
        return false;
    }
    return true;
}
