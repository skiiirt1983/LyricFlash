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
#include "power.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_system.h"

static const char *TAG = "LyricFlash";

/* Independent LVGL heartbeat + stalled-wake recovery. A normal four-hour
 * screen-off period is NOT a stall: only an explicitly awake UI within the
 * first 120 seconds after a wake is eligible. Never restart during OTA.
 * The separate recognition stall rule remains unchanged. */
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
        const bool wake_window=ui.screen_awake &&
             ui.last_wake_age_ms!=UINT32_MAX && ui.last_wake_age_ms<120000U;
        const bool wake_heartbeat_stalled=wake_window &&
             ui.ui_tick_age_ms!=UINT32_MAX && ui.ui_tick_age_ms>25000U;
        const bool wake_cover_stalled=wake_window &&
             ui.wake_overlay_pending && ui.last_wake_age_ms>45000U;
        if((wake_heartbeat_stalled||wake_cover_stalled) && uptime_ms>60000 &&
           !lf_network_ota_in_progress()){
            const uint8_t reason=wake_heartbeat_stalled?1U:2U;
            ESP_LOGE(TAG,"Wake UI stalled reason=%u age=%lu ms tick=%lu ms cover=%d; "
                         "saving recovery breadcrumb before settings-safe restart",
                     (unsigned)reason,(unsigned long)ui.last_wake_age_ms,
                     (unsigned long)ui.ui_tick_age_ms,ui.wake_overlay_pending);
            lf_diag_wake_recovery(reason,ui.last_wake_age_ms,ui.ui_tick_age_ms);
            vTaskDelay(pdMS_TO_TICKS(150));
            esp_restart();
        }
        if(uptime_ms>60000 && scan_related && song_page &&
           ui.ui_tick_age_ms!=UINT32_MAX &&
           ui.ui_tick_age_ms>25000U && !lf_network_ota_in_progress()){
            ESP_LOGE(TAG,"LVGL heartbeat lost %lu ms; state=%s page=%lu "
                     "cancel_phase=%lu render=%lu flush=%lu/%lu ready=%lu "
                     "restarting without wiping settings",
                     (unsigned long)ui.ui_tick_age_ms,
                     lf_recog_state_name(song.state),
                     (unsigned long)ui.ui_page,
                     (unsigned long)ui.record_cancel_phase,
                     (unsigned long)ui.render_count,
                     (unsigned long)ui.flush_finishes,
                     (unsigned long)ui.flush_starts,
                     (unsigned long)ui.refresh_ready);
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
    /* Pin the original 240 MHz before initializing the display, and
       release only on screen-off. Any initialization failure preserves
       the original clock policy. */
    if(!lf_power_cpu_profile_start())
        ESP_LOGW(TAG,"CPU screen-off scaling unavailable; original clock policy");
    /* 0.8.120: TE_SYNC forces LVGL FULL render mode on this QSPI panel, so
       every second-hand step, waveform tick and pager frame re-rendered all
       466x466 pixels from PSRAM. The UI is built around partial invalidation
       (pager strips, clipped clock hands), so use partial mode with
       double-buffered internal-SRAM strips that DMA directly to the panel. */
    /* 0.8.120: direct LVGL/esp_lcd port (no esp_lvgl_adapter). Benchmarks:
       blank full frame 47 ms through the adapter vs 13.8 ms panel transfer. */
    if (bsp_display_start_direct() == NULL) {
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
