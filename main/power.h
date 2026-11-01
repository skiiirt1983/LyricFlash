#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool valid;
    bool battery_present;
    bool usb_connected;
    bool charging;
    bool charge_complete;
    int percentage;  /* -1 when unknown */
} lf_power_state_t;

/* Battery telemetry stays read-only. All AXP2101 I2C transfers run on a
   background FreeRTOS task rather than blocking the LVGL render task. */
bool lf_power_read(lf_power_state_t *state);
bool lf_power_background_start(void);
bool lf_power_cached(lf_power_state_t *state);
uint32_t lf_power_color(const lf_power_state_t *state);
const char *lf_power_symbol(const lf_power_state_t *state);
