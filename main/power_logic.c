#include "power.h"
#include <stddef.h>

/* AXP2101 registers: PMU status1 0x00, status2 0x01, fuel gauge SOC 0xA4.
   PMU status1 bit5 indicates VBUS-good; bit3 battery-present.
   PMU status2 bits2:0 indicate charge phase (0-3 charging, 4 done).
   All values are read-only to avoid altering battery management. */
lf_power_state_t lf_power_decode(uint8_t s1, uint8_t s2, uint8_t soc)
{
    lf_power_state_t st = {
        .valid = true,
        .battery_present = (s1 & 0x08u) != 0,
        .usb_connected = (s1 & 0x20u) != 0,
        .charging = ((s2 & 0x07u) <= 3u) && ((s1 & 0x20u) != 0),
        .charge_complete = ((s2 & 0x07u) == 4u) && ((s1 & 0x20u) != 0),
        .percentage = (s1 & 0x08u) && soc <= 100 ? (int)soc : -1
    };
    return st;
}
uint32_t lf_power_color(const lf_power_state_t *s)
{
    if (!s || !s->valid) return 0x8C95A3;
    if (s->usb_connected) return 0x42DC89;
    if (s->percentage >= 0 && s->percentage < 10) return 0xFF5C64;
    if (s->percentage >= 0 && s->percentage < 20) return 0xFFB44C;
    return 0xDCE8F0;
}
const char *lf_power_symbol(const lf_power_state_t *s)
{
    if (!s || !s->valid) return "BAT ?";
    if (s->charging) return "CHARGING";
    if (s->charge_complete) return "USB FULL";
    if (s->usb_connected) return "USB POWER";
    return "BATTERY";
}
