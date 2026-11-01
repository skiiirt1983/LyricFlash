#pragma once
#include <stdbool.h>
#include <stddef.h>

/* Self-hosted Wi-Fi onboarding and authenticated local OTA uploads.
   Never enables an audio recording path. */
bool lf_network_start(void);
const char *lf_network_address(void);
const char *lf_network_ap_ssid(void);
const char *lf_network_ap_password(void);
const char *lf_network_ota_pin(void);
void lf_network_arm_ota(void);
bool lf_network_ota_armed(void);
