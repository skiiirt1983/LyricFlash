#include "diagnostics.h"
#include "network.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "nvs.h"
#include "esp_heap_caps.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include <stdio.h>
#include <string.h>

static portMUX_TYPE diag_mux = portMUX_INITIALIZER_UNLOCKED;
static char recent_errors[3][100];

void lf_diag_error(const char *component, const char *detail)
{
    char message[100];
    snprintf(message, sizeof message, "%s: %s", component ? component : "system",
             detail ? detail : "unknown error");
    /* Fixed-size copies inside a short critical section: no heap allocation. */
    portENTER_CRITICAL(&diag_mux);
    if (strcmp(recent_errors[0], message) == 0) {
        portEXIT_CRITICAL(&diag_mux);
        return;
    }
    memcpy(recent_errors[2], recent_errors[1], sizeof recent_errors[2]);
    memcpy(recent_errors[1], recent_errors[0], sizeof recent_errors[1]);
    memcpy(recent_errors[0], message, sizeof recent_errors[0]);
    portEXIT_CRITICAL(&diag_mux);
}

/* A few NVS writes only when a wake watchdog actually restarts the CPU.
   This preserves evidence across reboot and does not wear flash during normal
   60-second screen timeouts or 120-second Wi-Fi grace periods. */
void lf_diag_wake_recovery(uint8_t reason,uint32_t age_ms,uint32_t tick_age_ms)
{
    nvs_handle_t h;
    if(nvs_open("lf_wake_dbg",NVS_READWRITE,&h)!=ESP_OK)return;
    uint32_t count=0;
    (void)nvs_get_u32(h,"count",&count);
    if(count<UINT32_MAX)count++;
    esp_err_t err=nvs_set_u32(h,"count",count);
    if(err==ESP_OK)err=nvs_set_u8(h,"reason",reason);
    if(err==ESP_OK)err=nvs_set_u32(h,"wake_ms",age_ms);
    if(err==ESP_OK)err=nvs_set_u32(h,"tick_ms",tick_age_ms);
    if(err==ESP_OK)err=nvs_commit(h);
    nvs_close(h);
    if(err!=ESP_OK)ESP_LOGW("LFDiag","Wake recovery record failed: %s",esp_err_to_name(err));
}

void lf_diag_snapshot(lf_diag_snapshot_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->uptime_seconds = (uint32_t)(esp_timer_get_time() / 1000000LL);
    out->heap_free_bytes = esp_get_free_heap_size();
    out->heap_min_bytes = esp_get_minimum_free_heap_size();
    out->heap_largest_block_bytes = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    out->reset_reason = (uint32_t)esp_reset_reason();
    out->task_count = (uint32_t)uxTaskGetNumberOfTasks();
    out->psram_free_bytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    out->wifi_rssi_dbm = 0;
    out->wifi_connected = lf_network_is_connected();
    if (out->wifi_connected) {
        wifi_ap_record_t ap = {0};
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            out->wifi_rssi_dbm = ap.rssi;
            snprintf(out->wifi_ssid, sizeof out->wifi_ssid, "%.32s",
                     (const char *)ap.ssid);
        }
    }
    snprintf(out->ip_address, sizeof out->ip_address, "%s", lf_network_address());
    out->battery_valid = lf_power_cached(&out->battery);
    /* Read in the HTTP worker, never the latency-sensitive LVGL wake path. */
    nvs_handle_t wake_h;
    if(nvs_open("lf_wake_dbg",NVS_READONLY,&wake_h)==ESP_OK){
        (void)nvs_get_u32(wake_h,"count",&out->wake_recovery_count);
        (void)nvs_get_u8(wake_h,"reason",&out->wake_recovery_last_reason);
        (void)nvs_get_u32(wake_h,"wake_ms",&out->wake_recovery_last_age_ms);
        (void)nvs_get_u32(wake_h,"tick_ms",&out->wake_recovery_last_tick_age_ms);
        nvs_close(wake_h);
    }
    portENTER_CRITICAL(&diag_mux);
    memcpy(out->errors, recent_errors, sizeof out->errors);
    portEXIT_CRITICAL(&diag_mux);
}
