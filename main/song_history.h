#pragma once
#include <stdbool.h>
#include <stddef.h>
/* Read-only history export and append-only local identification index.
   Existing lf_songs lyric and timing keys are NEVER overwritten or deleted. */
typedef struct {
    char title[128];
    char artist[128];
    bool lyrics_cached;
} lf_song_history_item_t;
/* Returns number of items on this page; total is the number known on-device.
   Legacy lyric-cache songs are visible even before the history index existed. */
size_t lf_song_history_list(lf_song_history_item_t *out,size_t cap,
                            size_t offset,size_t *total);
bool lf_song_history_remember(const char *title,const char *artist);
