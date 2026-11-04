#pragma once

/* Confirm a newly installed image only after the running LVGL task proves
   responsive across multiple post-boot samples. Never gate on Internet
   availability: the local OTA recovery server may run in setup AP mode.
   Verified 0.8.67 remains the rollback image until confirmation. */
bool lf_ota_guard_start_health_check(void);
void lf_ota_guard_confirm_startup(void);
void lf_ota_guard_failure(const char *reason);
