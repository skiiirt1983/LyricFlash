#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 16-bit stereo WAV, MIC1 and MIC2 from interleaved four-slot ES7210 TDM.
 * Pure helpers with no ESP dependencies, covered by native host tests. */
#define LF_SCAN_RATE 24000U
#define LF_SCAN_SECONDS 10U
#define LF_SCAN_CHANNELS 2U
#define LF_SCAN_MAX_FRAMES (LF_SCAN_RATE * LF_SCAN_SECONDS)
#define LF_SCAN_WAV_HEADER 44U
#define LF_SCAN_WAV_BYTES (LF_SCAN_WAV_HEADER + LF_SCAN_MAX_FRAMES * 4U)
#define LF_SCAN_TDM_SAMPLES 512U
#define LF_SCAN_TDM_FRAMES (LF_SCAN_TDM_SAMPLES / 4U)

/* Diagnostics are computed from the same MIC1/MIC2 TDM frames sent to AudD.
 * Only aggregate counts are stored; audio never enters the status API. */
#define LF_SCAN_NEAR_RAIL 32760U
#define LF_SCAN_HALF_SCALE 16384U
typedef struct {
    uint64_t mic1_sum_sq, mic2_sum_sq;
    uint64_t mic1_below_rail_sq, mic2_below_rail_sq;
    uint32_t frames;
    uint32_t mic1_peak, mic2_peak;
    uint32_t mic1_near_rail, mic2_near_rail, both_near_rail;
    uint32_t mic1_half_scale, mic2_half_scale;
    uint32_t mic1_rail_bursts, mic2_rail_bursts;
    uint32_t mic1_longest_burst, mic2_longest_burst;
    uint32_t mic1_rail_run, mic2_rail_run;
    uint32_t mic1_below_rail_frames, mic2_below_rail_frames;
} lf_scan_audio_metrics_t;
void lf_scan_audio_metrics_add(lf_scan_audio_metrics_t *metrics,
                               const int16_t *tdm, size_t samples);
uint32_t lf_scan_audio_rms(uint64_t sum_squares, uint32_t frames);

void lf_scan_wav_header(uint8_t header[44], uint32_t frames);
size_t lf_scan_tdm_to_stereo(uint8_t *dest, size_t cap,
                             const int16_t *tdm, size_t samples);
bool lf_scan_token_valid(const char *token);
