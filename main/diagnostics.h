#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "power.h"

typedef struct {
    uint32_t uptime_seconds;
    uint32_t heap_free_bytes;
    uint32_t heap_min_bytes;
    uint32_t heap_largest_block_bytes;
    uint32_t reset_reason;
    uint32_t task_count;
    uint32_t psram_free_bytes;
    bool wifi_connected;
    int wifi_rssi_dbm;
    char wifi_ssid[33];
    char ip_address[32];
    bool battery_valid;
    lf_power_state_t battery;
    char errors[3][100];
} lf_diag_snapshot_t;

/* Bounded, application-owned event log; never includes credentials or tokens. */
void lf_diag_error(const char *component, const char *detail);
void lf_diag_snapshot(lf_diag_snapshot_t *out);
