#include "diagnostics.h"
#include "network.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_wifi.h"
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
    portENTER_CRITICAL(&diag_mux);
    memcpy(out->errors, recent_errors, sizeof out->errors);
    portEXIT_CRITICAL(&diag_mux);
}
