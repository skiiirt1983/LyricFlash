#pragma once
#include <stdbool.h>
#include <stdint.h>
bool lf_sync_parse_timecode_ms(const char *value, uint32_t *out_ms);
uint32_t lf_sync_estimate_position_ms(uint32_t audd_ms,uint64_t capture_start_ms,uint32_t frames,uint32_t rate,uint64_t now_ms);
/* Observed ~1s early display, independent of saved manual clock correction. */
#define LF_SYNC_DISPLAY_DELAY_MS 1000U
uint32_t lf_sync_visible_position_ms(uint32_t estimated_ms);
uint16_t lf_sync_slide_fraction(uint32_t position_ms,uint32_t start_ms,uint32_t next_ms);
uint16_t lf_sync_row_pitch_px(uint16_t height_a,uint16_t height_b,uint32_t gap_ms);
