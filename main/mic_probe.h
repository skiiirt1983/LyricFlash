#pragma once
#include <stdint.h>
#include <stdbool.h>

/* First hardware-test stage: manually probe the ES7210 ADC's I2C ACK.
 * No microphone sampling, DMA, codec register writes or audio uploads.
 * A positive probe proves the ADC responds, NOT that sound capture works. */
typedef enum {
    LF_MIC_NEVER_TESTED = 0,
    LF_MIC_PRESENT,
    LF_MIC_NOT_DETECTED,
    LF_MIC_BUS_NOT_READY,
    LF_MIC_BUS_ERROR,
    LF_MIC_BUSY
} lf_mic_probe_result_t;

typedef struct {
    lf_mic_probe_result_t result;
    int error_code;
    uint32_t attempts;
} lf_mic_probe_snapshot_t;

lf_mic_probe_result_t lf_mic_probe_run(void);
void lf_mic_probe_snapshot(lf_mic_probe_snapshot_t *out);
const char *lf_mic_probe_result_text(lf_mic_probe_result_t result);
