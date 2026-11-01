#include "power.h"
#include <assert.h>
#include <stdio.h>
extern lf_power_state_t lf_power_decode(uint8_t, uint8_t, uint8_t);
int main(void) {
    lf_power_state_t b = lf_power_decode(0x08, 5, 12);
    assert(b.valid && b.battery_present && !b.usb_connected && !b.charging);
    assert(b.percentage == 12 && lf_power_color(&b) == 0xFFB44C);
    b = lf_power_decode(0x28, 2, 77);
    assert(b.usb_connected && b.charging && b.percentage == 77);
    assert(lf_power_color(&b) == 0x42DC89);
    b = lf_power_decode(0x28, 4, 100);
    assert(b.charge_complete && !b.charging);
    b = lf_power_decode(0x00, 5, 255);
    assert(!b.battery_present && b.percentage == -1);
    puts("PASS: AXP2101 charging / battery states / display colours");
    return 0;
}
