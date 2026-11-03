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

void lf_scan_wav_header(uint8_t header[44], uint32_t frames);
size_t lf_scan_tdm_to_stereo(uint8_t *dest, size_t cap,
                             const int16_t *tdm, size_t samples);
bool lf_scan_token_valid(const char *token);
