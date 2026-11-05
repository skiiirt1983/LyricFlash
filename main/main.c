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
#include "recognition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_system.h"

static const char *TAG = "LyricFlash";

/* A physically frozen AMOLED with a responsive Web UI is an LVGL worker
 * stall, not proof that AudD failed. An independent, low-priority task
 * monitors the lock-free heartbeat. Keep the cancelled recording in scope
 * for 90 seconds: its worker may move to STOPPED before the LVGL task hangs.
 * Only a >25s heartbeat loss on a song page causes a clean restart with
 * preserved NVS, credentials and OTA partitions. No new microphone access.
 * Never reset merely because AudD/LRCLIB is slow. */
static void lyricflash_display_recovery_watchdog(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(20000));
    for(;;){
        lf_ui_display_stats_t ui={0};
        lf_ui_display_stats(&ui); /* atomics only: no LVGL mutex */
        lf_recog_snapshot_t song={0};
        lf_recog_snapshot(&song);
        const int64_t uptime_ms=esp_timer_get_time()/1000LL;
        const bool recent_cancel=ui.last_record_cancel_age_ms!=UINT32_MAX &&
                                 ui.last_record_cancel_age_ms<90000U;
        const bool scan_related=song.attempts>0 &&
            (song.state==LF_RECOG_UPLOAD ||
             song.state==LF_RECOG_LYRICS ||
             song.state==LF_RECOG_MATCH || recent_cancel);
        const bool song_page=ui.ui_page==0 || ui.ui_page==1 || ui.ui_page==9;
        if(uptime_ms>60000 && scan_related && song_page &&
           ui.ui_tick_age_ms!=UINT32_MAX &&
           ui.ui_tick_age_ms>25000U){
            ESP_LOGE(TAG,"LVGL heartbeat lost for %lu ms while recognition "
                     "state=%s, page=%lu; restarting without wiping settings",
                     (unsigned long)ui.ui_tick_age_ms,
                     lf_recog_state_name(song.state),
                     (unsigned long)ui.ui_page);
            vTaskDelay(pdMS_TO_TICKS(150));
            esp_restart();
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

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
    /* Isolate 0.8.16's second-flush TE deadlock: keep the PrintSphere-derived
       CO5300 PSRAM DMA staging and SPI queue depth 1, but disable TE waiting
       while keeping the same core-1 LVGL worker and application. */
    bsp_display_cfg_t display_cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE,
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
    /* OTA image remains PENDING_VERIFY until an independent task confirms
       live LVGL rendering and a healthy heap across several samples. The
       existing known-good image stays recoverable until that point. */
    if (!lf_ota_guard_start_health_check())
        ESP_LOGE(TAG, "Post-OTA health verifier could not start");
    /* Do not impede boot or OTA if this optional recovery task cannot start. */
    if(xTaskCreate(lyricflash_display_recovery_watchdog,
                   "lf_ui_heartbeat",3072,NULL,tskIDLE_PRIORITY+1,NULL)!=pdPASS)
        ESP_LOGW(TAG,"Display recovery monitor unavailable");
}
