#pragma once
#include <stdbool.h>
#include <stdint.h>
bool lf_sync_parse_timecode_ms(const char *value, uint32_t *out_ms);
uint32_t lf_sync_estimate_position_ms(uint32_t audd_ms,uint64_t capture_start_ms,uint32_t frames,uint32_t rate,uint64_t now_ms);
