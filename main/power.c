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


/* 0.8.124: attach the PMIC once and keep the handle; adding/removing a
   device on the shared touch I2C bus every 4 s was needless churn. */
static i2c_master_dev_handle_t pmic_dev(void)
{
    static i2c_master_dev_handle_t dev = NULL;
    if (dev) return dev;
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) return NULL;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x34,
        .scl_speed_hz = 100000
    };
    if (i2c_master_bus_add_device(bus, &cfg, &dev) != ESP_OK) dev = NULL;
    return dev;
}
static bool pmic_read(uint8_t reg, uint8_t *value)
{
    i2c_master_dev_handle_t dev = pmic_dev();
    return dev && i2c_master_transmit_receive(dev, &reg, 1, value, 1, 100) == ESP_OK;
}
static bool pmic_write(uint8_t reg, uint8_t value)
{
    i2c_master_dev_handle_t dev = pmic_dev();
    const uint8_t frame[2] = {reg, value};
    return dev && i2c_master_transmit(dev, frame, sizeof frame, 100) == ESP_OK;
}

/* ---- 1.1.1 PWR button (AXP2101 power key) ----
 * Register use follows the AXP2101 map (as in XPowersLib):
 *   0x27 key timing: bits 1:0 power-on press (00 = 128 ms), bits 3:2 hardware
 *        power-off hold (00 = 4 s), bits 5:4 long-press report (01 = 1.5 s)
 *   0x22 bit1 hardware long-press power-off enabled, bit0 0 = off (not restart)
 *   0x41 / 0x49 interrupt enable / status: bit3 short press, bit2 long press
 *   0x10 bit0 software power off
 * A quick tap is reported as a short press (sleep / wake); holding 1.5 s is a
 * long press (power off). The chip's own 4 s hold still cuts power if the
 * firmware is ever stuck. */
#define AXP_KEY_TIMING 0x27
#define AXP_PWROFF_EN 0x22
#define AXP_IRQ_EN2 0x41
#define AXP_IRQ_STATUS2 0x49
#define AXP_COMMON 0x10
#define AXP_KEY_SHORT 0x08
#define AXP_KEY_LONG 0x04
static atomic_int key_event; /* 0 none, 1 short, 2 long */
static bool key_ready;
static void power_key_setup(void)
{
    uint8_t v = 0;
    if (!pmic_read(AXP_KEY_TIMING, &v)) return;
    if (!pmic_write(AXP_KEY_TIMING, (uint8_t)((v & 0xC0) | 0x10))) return;
    if (pmic_read(AXP_PWROFF_EN, &v)) (void)pmic_write(AXP_PWROFF_EN, (uint8_t)((v | 0x02) & ~0x01));
    if (!pmic_read(AXP_IRQ_EN2, &v)) return;
    if (!pmic_write(AXP_IRQ_EN2, (uint8_t)(v | AXP_KEY_SHORT | AXP_KEY_LONG))) return;
    /* Forget the press that switched the device on. */
    (void)pmic_write(AXP_IRQ_STATUS2, AXP_KEY_SHORT | AXP_KEY_LONG);
    key_ready = true;
    ESP_LOGI("LFPower", "PWR button: tap = sleep/wake, hold 1.5 s = power off");
}
static void power_key_poll(void)
{
    if (!key_ready) { power_key_setup(); return; }
    uint8_t status = 0;
    if (!pmic_read(AXP_IRQ_STATUS2, &status)) return;
    const uint8_t keys = status & (AXP_KEY_SHORT | AXP_KEY_LONG);
    if (!keys) return;
    (void)pmic_write(AXP_IRQ_STATUS2, keys); /* write 1 to clear */
    atomic_store(&key_event, (keys & AXP_KEY_LONG) ? 2 : 1);
}
int lf_power_take_key_event(void) { return atomic_exchange(&key_event, 0); }
void lf_power_off(void)
{
    uint8_t v = 0;
    if (pmic_read(AXP_COMMON, &v)) (void)pmic_write(AXP_COMMON, (uint8_t)(v | 0x01));
}

bool lf_power_read(lf_power_state_t *state)
{
    if (!state) return false;
    memset(state, 0, sizeof(*state));
    state->percentage = -1;
    i2c_master_dev_handle_t dev = pmic_dev();
    if (!dev) return false;
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
    /* Battery every 4 s; the PWR button every 100 ms (one 1-byte read). */
    for (unsigned tick = 0;; tick++) {
        if (tick % 40U == 0) {
            lf_power_state_t sample;
            if (lf_power_read(&sample)) {
                portENTER_CRITICAL(&power_mux);
                latest_power = sample;
                has_power = true;
                portEXIT_CRITICAL(&power_mux);
            }
        }
        power_key_poll();
        vTaskDelay(pdMS_TO_TICKS(100));
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
