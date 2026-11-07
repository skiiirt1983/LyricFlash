#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Local device Web UI and OTA. Wi-Fi setup supports browser and touch.
   No PIN or on-device OTA arming is required. */
bool lf_network_start(void);
const char *lf_network_address(void);
const char *lf_network_ap_ssid(void);
const char *lf_network_ap_password(void);

#define LF_WIFI_MAX_RESULTS 12
typedef struct {
    char ssid[33];
    int8_t rssi;
    bool secured;
} lf_wifi_ap_t;

/* Nonblocking scan; status: 0 idle, 1 scanning, 2 complete, -1 error.
   Copies results to caller. Safe to poll from the LVGL UI task. */
bool lf_network_scan_start(void);
int lf_network_scan_results(lf_wifi_ap_t *out, size_t max, size_t *count);
bool lf_network_save_wifi(const char *ssid, const char *password);
void lf_network_restart(void);
bool lf_network_is_connected(void);
bool lf_network_connected_ssid(char *out, size_t size);

const char *lf_network_ota_result(void);
uint32_t lf_network_ota_received(void);
uint32_t lf_network_ota_total(void);

/* Request Wi-Fi radio state without blocking the LVGL rendering task.
   Sleep suspension is ignored during OTA and setup AP provisioning. */
void lf_network_set_idle(bool idle);

/* Read-only power state and radio-off count for on-device diagnostics. */
const char *lf_network_wifi_power_state(void);
uint32_t lf_network_radio_sleep_cycles(void);
/* Background flash operation guard for independent UI recovery monitor. */
bool lf_network_ota_in_progress(void);
