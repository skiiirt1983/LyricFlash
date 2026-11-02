#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Local device web manager + PIN-armed OTA. The ESP32 also supports
   touchscreen Wi-Fi scanning/provisioning without joining its setup AP. */
bool lf_network_start(void);
const char *lf_network_address(void);
const char *lf_network_ap_ssid(void);
const char *lf_network_ap_password(void);
const char *lf_network_ota_pin(void);
void lf_network_arm_ota(void);
bool lf_network_ota_armed(void);

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
