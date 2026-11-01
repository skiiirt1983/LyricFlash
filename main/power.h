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

/* Read-only AXP2101 telemetry over Waveshare BSP's shared I2C bus. */
bool lf_power_read(lf_power_state_t *state);
uint32_t lf_power_color(const lf_power_state_t *state);
const char *lf_power_symbol(const lf_power_state_t *state);
