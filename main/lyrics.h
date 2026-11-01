#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define LF_MAX_LINES 160
#define LF_MAX_LINE_BYTES 192

typedef struct {
    uint32_t start_ms;
    char text[LF_MAX_LINE_BYTES];
} lf_lyric_line_t;

typedef struct {
    size_t count;
    lf_lyric_line_t line[LF_MAX_LINES];
} lf_lyrics_t;

// Parse standard [mm:ss.xx] LRC; repeat tags on a line are supported.
// Unsupported metadata and malformed timestamps are skipped.
// Text is UTF-8 bytes, shortened at UTF-8 codepoint boundary if necessary.
size_t lf_parse_lrc(const char *input, lf_lyrics_t *out);
// Returns line index currently sung, or -1 if nothing has started.
int lf_lyric_at_ms(const lf_lyrics_t *lyrics, uint32_t time_ms);
// Fraction of playback duration elapsed, capped to [0,1].
float lf_lyric_progress(uint32_t position_ms, uint32_t duration_ms, int offset_ms);
