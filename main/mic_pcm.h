#pragma once
#include <stdbool.h>
#include <stdint.h>

/* One bounded, explicitly requested audio test. No raw audio storage or upload.
 * start() spawns a task, read-only status is safe from the HTTP server.
 * stop() requests an early exit; read loops must honor it. */
typedef enum {
    LF_PCM_IDLE=0, LF_PCM_RUNNING=1, LF_PCM_COMPLETE=2, LF_PCM_ERROR=3
} lf_pcm_state_t;
typedef enum {
    LF_PCM_ERR_NONE=0, LF_PCM_ERR_TASK=1, LF_PCM_ERR_CODEC=2,
    LF_PCM_ERR_OPEN=3, LF_PCM_ERR_READ=4, LF_PCM_ERR_EMPTY=5,
    LF_PCM_ERR_GAIN=6
} lf_pcm_error_t;
typedef struct {
    lf_pcm_state_t state;
    lf_pcm_error_t error;
    uint32_t attempts;
    uint32_t sample_count;
    uint32_t peak_abs;
    uint32_t mean_abs;
    uint32_t mic1_mean_abs, mic1_peak_abs;
    uint32_t mic2_mean_abs, mic2_peak_abs;
    uint32_t elapsed_ms;
    bool stop_requested;
} lf_pcm_snapshot_t;

bool lf_pcm_start(void);
void lf_pcm_stop(void);
void lf_pcm_snapshot(lf_pcm_snapshot_t *out);
const char *lf_pcm_state_name(lf_pcm_state_t state);
const char *lf_pcm_error_name(lf_pcm_error_t error);
