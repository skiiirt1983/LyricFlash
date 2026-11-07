#include "power.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_pm.h"
#include <stdatomic.h>
#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

extern lf_power_state_t lf_power_decode(uint8_t status1, uint8_t status2, uint8_t soc);

/* Keep 240 MHz while AMOLED is in use; allow 80 MHz in screen-off idle.
 * Never enable automatic light sleep: CST9217 touch, QSPI, Wi-Fi and OTA
 * remain alive. Wi-Fi default min modem sleep is left unchanged. */
static esp_pm_lock_handle_t screen_cpu_lock;
static atomic_bool screen_cpu_lock_held;

bool lf_power_cpu_profile_start(void)
{
    if(screen_cpu_lock)return atomic_load(&screen_cpu_lock_held);
    esp_pm_config_t profile={
        .max_freq_mhz=240,
        .min_freq_mhz=80,
        .light_sleep_enable=false
    };
    esp_err_t err=esp_pm_configure(&profile);
    if(err!=ESP_OK){
        ESP_LOGW("LFPower","CPU power profile unavailable: %s",esp_err_to_name(err));
        return false;
    }
    err=esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX,0,"lf_awake",&screen_cpu_lock);
    if(err==ESP_OK)err=esp_pm_lock_acquire(screen_cpu_lock);
    if(err!=ESP_OK){
        ESP_LOGW("LFPower","Cannot pin awake CPU: %s",esp_err_to_name(err));
        if(screen_cpu_lock){
            esp_pm_lock_delete(screen_cpu_lock);
            screen_cpu_lock=NULL;
        }
        /* Fail safe: the active GUI must retain its former clock. */
        profile.min_freq_mhz=240;
        (void)esp_pm_configure(&profile);
        return false;
    }
    atomic_store(&screen_cpu_lock_held,true);
    ESP_LOGI("LFPower","CPU 240 MHz awake; 80 MHz minimum screen-off; no auto light sleep");
    return true;
}
void lf_power_set_screen_awake(bool awake)
{
    if(!screen_cpu_lock||awake==atomic_load(&screen_cpu_lock_held))return;
    esp_err_t err=awake?esp_pm_lock_acquire(screen_cpu_lock):
                         esp_pm_lock_release(screen_cpu_lock);
    if(err!=ESP_OK){
        ESP_LOGW("LFPower","CPU profile transition failed: %s",esp_err_to_name(err));
        return;
    }
    atomic_store(&screen_cpu_lock_held,awake);
}
const char *lf_power_cpu_profile_state(void)
{
    if(!screen_cpu_lock)return "fixed_240";
    return atomic_load(&screen_cpu_lock_held)?"awake_240":"screen_off_dfs_80";
}


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
    /* 0.8.124: attach the PMIC once and keep the handle; adding/removing a
       device on the shared touch I2C bus every 4 s was needless churn. */
    static i2c_master_dev_handle_t dev = NULL;
    if (!dev && i2c_master_bus_add_device(bus, &cfg, &dev) != ESP_OK) { dev = NULL; return false; }
    uint8_t status1 = 0, status2 = 0, soc = 0;
    const uint8_t r0 = 0x00, r1 = 0x01, ra = 0xA4;
    esp_err_t a = i2c_master_transmit_receive(dev, &r0, 1, &status1, 1, 100);
    esp_err_t b = i2c_master_transmit_receive(dev, &r1, 1, &status2, 1, 100);
    esp_err_t c = i2c_master_transmit_receive(dev, &ra, 1, &soc, 1, 100);
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
