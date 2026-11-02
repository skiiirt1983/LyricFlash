#include "power.h"
#include "esp_err.h"
#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

extern lf_power_state_t lf_power_decode(uint8_t status1, uint8_t status2, uint8_t soc);

bool lf_power_read(lf_power_state_t *state)
{
    if (!state) return false;
    memset(state, 0, sizeof(*state));
    state->percentage = -1;
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) return false;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x34,
        .scl_speed_hz = 100000
    };
    i2c_master_dev_handle_t dev = NULL;
    if (i2c_master_bus_add_device(bus, &cfg, &dev) != ESP_OK) return false;
    uint8_t status1 = 0, status2 = 0, soc = 0;
    const uint8_t r0 = 0x00, r1 = 0x01, ra = 0xA4;
    esp_err_t a = i2c_master_transmit_receive(dev, &r0, 1, &status1, 1, 100);
    esp_err_t b = i2c_master_transmit_receive(dev, &r1, 1, &status2, 1, 100);
    esp_err_t c = i2c_master_transmit_receive(dev, &ra, 1, &soc, 1, 100);
    (void)i2c_master_bus_rm_device(dev);
    if (a != ESP_OK || b != ESP_OK || c != ESP_OK) return false;
    *state = lf_power_decode(status1, status2, soc);
    return true;
}

/* The display should never wait for the shared I2C bus.
   A separate low-priority task samples once every four seconds. */
static portMUX_TYPE power_mux = portMUX_INITIALIZER_UNLOCKED;
static lf_power_state_t latest_power;
static bool has_power;
static TaskHandle_t power_worker;

static void power_background_task(void *arg)
{
    (void)arg;
    for (;;) {
        lf_power_state_t sample;
        if (lf_power_read(&sample)) {
            portENTER_CRITICAL(&power_mux);
            latest_power = sample;
            has_power = true;
            portEXIT_CRITICAL(&power_mux);
        }
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

bool lf_power_background_start(void)
{
    if (power_worker) return true;
    return xTaskCreate(power_background_task, "lf_power", 3072, NULL,
                       tskIDLE_PRIORITY + 1, &power_worker) == pdPASS;
}

bool lf_power_cached(lf_power_state_t *state)
{
    if (!state) return false;
    portENTER_CRITICAL(&power_mux);
    bool ok = has_power;
    if (ok) *state = latest_power;
    portEXIT_CRITICAL(&power_mux);
    return ok;
}
