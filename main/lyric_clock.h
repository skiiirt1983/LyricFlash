#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Manual external-player alignment. AudD gives a song ID, not the current
 * playback position. No audio monitoring, network calls, or drift claims. */
typedef struct {
    uint64_t origin_ms;
    uint32_t elapsed_ms;
    int32_t correction_ms;
    bool started;
    bool running;
} lf_lyric_clock_t;
void lf_lyric_clock_start(lf_lyric_clock_t *c, uint64_t now);
void lf_lyric_clock_pause(lf_lyric_clock_t *c, uint64_t now);
void lf_lyric_clock_resume(lf_lyric_clock_t *c, uint64_t now);
void lf_lyric_clock_adjust(lf_lyric_clock_t *c, int32_t delta_ms);
uint32_t lf_lyric_clock_position(const lf_lyric_clock_t *c, uint64_t now);
int lf_lyric_clock_line_at(const uint32_t *timestamps, size_t count, uint32_t position_ms);
