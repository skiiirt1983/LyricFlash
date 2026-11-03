#pragma once
#include "lyrics.h"
#include <stdbool.h>

/* Lyrics are fetched ONLY following explicit, successful user-requested AudD
 * recognition. No LRCLIB key is needed. Distinguish 429 from 404, without
 * automatic retries. The caller owns PSRAM-backed output. */
typedef enum {
 LF_LRCLIB_SYNCED=0, LF_LRCLIB_PLAIN, LF_LRCLIB_INSTRUMENTAL,
 LF_LRCLIB_NOT_FOUND, LF_LRCLIB_RATE_LIMIT, LF_LRCLIB_UNAVAILABLE,
 LF_LRCLIB_CANCELLED
} lf_lrclib_state_t;
lf_lrclib_state_t lf_lrclib_lookup(const char *track, const char *artist,
                                  lf_lyrics_t *out,
                                  bool (*is_cancelled)(void));
const char *lf_lrclib_state_name(lf_lrclib_state_t state);
