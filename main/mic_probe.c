#include "mic_probe.h"
#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdatomic.h>

#define LF_ES7210_I2C_ADDR (0x40)
#define LF_MIC_PROBE_TIMEOUT_MS (100)

/* Tests are requested manually through the local Web UI. The audio path
 * stays inactive at boot and throughout this test, including on failure.
 * The I2C bus is already owned and initialized by the Waveshare BSP.
 * Never reinitialize that bus or disturb the AXP2101/touch peripherals. */
static atomic_int last_result = ATOMIC_VAR_INIT(LF_MIC_NEVER_TESTED);
static atomic_int last_error = ATOMIC_VAR_INIT(0);
static atomic_uint attempts = ATOMIC_VAR_INIT(0);
static atomic_flag probe_lock = ATOMIC_FLAG_INIT;

const char *lf_mic_probe_result_text(lf_mic_probe_result_t result)
{
    switch (result) {
    case LF_MIC_NEVER_TESTED: return "not_tested";
    case LF_MIC_PRESENT: return "adc_detected";
    case LF_MIC_NOT_DETECTED: return "adc_not_detected";
    case LF_MIC_BUS_NOT_READY: return "i2c_not_ready";
    case LF_MIC_BUS_ERROR: return "i2c_error";
    case LF_MIC_BUSY: return "test_busy";
    default: return "unknown";
    }
}

void lf_mic_probe_snapshot(lf_mic_probe_snapshot_t *out)
{
    if (!out) return;
    out->result = (lf_mic_probe_result_t)atomic_load(&last_result);
    out->error_code = atomic_load(&last_error);
    out->attempts = atomic_load(&attempts);
}

lf_mic_probe_result_t lf_mic_probe_run(void)
{
    if (atomic_flag_test_and_set(&probe_lock)) return LF_MIC_BUSY;
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    esp_err_t err = bus ? i2c_master_probe(bus, LF_ES7210_I2C_ADDR,
                                          LF_MIC_PROBE_TIMEOUT_MS)
                        : ESP_ERR_INVALID_STATE;
    lf_mic_probe_result_t result;
    if (!bus) result = LF_MIC_BUS_NOT_READY;
    else if (err == ESP_OK) result = LF_MIC_PRESENT;
    else if (err == ESP_ERR_NOT_FOUND) result = LF_MIC_NOT_DETECTED;
    else result = LF_MIC_BUS_ERROR;
    atomic_store(&last_error, (int)err);
    atomic_fetch_add(&attempts, 1);
    atomic_store(&last_result, (int)result);
    atomic_flag_clear(&probe_lock);
    return result;
}
