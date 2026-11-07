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
    uint32_t wake_recovery_count;
    uint8_t wake_recovery_last_reason; /* 1 UI heartbeat, 2 wake cover */
    uint32_t wake_recovery_last_age_ms;
    uint32_t wake_recovery_last_tick_age_ms;
} lf_diag_snapshot_t;

/* Bounded, application-owned event log; never includes credentials or tokens. */
void lf_diag_error(const char *component, const char *detail);
void lf_diag_snapshot(lf_diag_snapshot_t *out);
/* Persist only verified wake watchdog restarts, never every wake/tick. */
void lf_diag_wake_recovery(uint8_t reason,uint32_t age_ms,uint32_t tick_age_ms);
