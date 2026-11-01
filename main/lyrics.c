#include "lyrics.h"
#include <stdlib.h>
#include <string.h>

static bool digit(char c) { return c >= '0' && c <= '9'; }

static bool time_tag(const char *s, const char *end, const char **after, uint32_t *ms)
{
    if (s >= end || *s++ != '[') return false;
    if (end - s < 5 || !digit(s[0]) || !digit(s[1])) return false;
    uint32_t minutes = 0;
    unsigned count = 0;
    while (s < end && digit(*s) && count < 3) {
        minutes = 10 * minutes + (*s - '0'); s++; count++;
    }
    if (s >= end || *s++ != ':' || end - s < 3 || !digit(s[0]) || !digit(s[1])) return false;
    uint32_t sec = (uint32_t)(s[0] - '0') * 10U + (s[1] - '0');
    s += 2;
    if (sec >= 60) return false;
    uint32_t fract = 0;
    if (s < end && *s == '.') {
        s++;
        unsigned digits = 0;
        while (s < end && digit(*s) && digits < 3) {
            fract = 10 * fract + (*s - '0'); s++; digits++;
        }
        if (digits == 0 || (s < end && digit(*s))) return false;
        if (digits == 1) fract *= 100;
        if (digits == 2) fract *= 10;
    }
    if (s >= end || *s++ != ']') return false;
    *ms = (minutes * 60U + sec) * 1000U + fract;
    *after = s;
    return true;
}

static void utf8_copy(char *dst, size_t cap, const char *begin, size_t bytes)
{
    if (bytes >= cap) bytes = cap - 1;
    // Avoid ending inside a UTF-8 continuation sequence.
    while (bytes && ((unsigned char)begin[bytes] & 0xc0) == 0x80) bytes--;
    memcpy(dst, begin, bytes);
    dst[bytes] = 0;
}

static int compare_line(const void *pa, const void *pb)
{
    const lf_lyric_line_t *a = pa, *b = pb;
    return (a->start_ms > b->start_ms) - (a->start_ms < b->start_ms);
}

size_t lf_parse_lrc(const char *input, lf_lyrics_t *out)
{
    if (!out) return 0;
    out->count = 0;
    if (!input) return 0;
    const char *p = input;
    while (*p && out->count < LF_MAX_LINES) {
        const char *end = strchr(p, '\n');
        if (!end) end = p + strlen(p);
        const char *cursor = p;
        uint32_t times[8]; size_t nt = 0;
        uint32_t t;
        const char *after;
        while (nt < 8 && time_tag(cursor, end, &after, &t)) {
            times[nt++] = t; cursor = after;
        }
        if (nt) {
            const char *last = end;
            while (last > cursor && (last[-1] == '\r' || last[-1] == ' ')) last--;
            while (cursor < last && *cursor == ' ') cursor++;
            for (size_t i = 0; i < nt && out->count < LF_MAX_LINES; i++) {
                lf_lyric_line_t *l = &out->line[out->count++];
                l->start_ms = times[i];
                utf8_copy(l->text, sizeof l->text, cursor, (size_t)(last - cursor));
            }
        }
        p = *end == '\n' ? end + 1 : end;
    }
    qsort(out->line, out->count, sizeof out->line[0], compare_line);
    return out->count;
}

int lf_lyric_at_ms(const lf_lyrics_t *lyrics, uint32_t time_ms)
{
    if (!lyrics || !lyrics->count || time_ms < lyrics->line[0].start_ms) return -1;
    size_t lo = 0, hi = lyrics->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (lyrics->line[mid].start_ms <= time_ms) lo = mid + 1;
        else hi = mid;
    }
    return (int)lo - 1;
}

float lf_lyric_progress(uint32_t position_ms, uint32_t duration_ms, int offset_ms)
{
    if (!duration_ms) return 0.f;
    int64_t adjusted = (int64_t)position_ms + offset_ms;
    if (adjusted <= 0) return 0.f;
    if (adjusted >= duration_ms) return 1.f;
    return (float)adjusted / (float)duration_ms;
}
