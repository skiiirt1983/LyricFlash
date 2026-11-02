#include <stdbool.h>
#pragma once
#include "settings.h"
#include "esp_err.h"
bool lf_ui_start(lf_settings_t *settings);

esp_err_t lf_ui_apply_basic_settings(const lf_settings_t *basic);

uint32_t lf_ui_max_tick_gap_ms(void);

/* Apply browser-editable global device preferences without losing clock profiles. */
esp_err_t lf_ui_apply_device_settings(const lf_settings_t *requested);

/* Called after Wi-Fi/update services initialize. Defer OTA confirmation until
   the real LVGL timer has remained responsive for 15 seconds. */
void lf_ui_mark_services_ready(void);
