#pragma once
#include "lyrics.h"
#include <stdbool.h>
#include <stdint.h>
/* NVS-backed, no-eviction per-track cache; the original OTA layout stays
   untouched. All functions are called outside LVGL critical sections. */
bool lf_song_cache_load_lyrics(const char *title,const char *artist,lf_lyrics_t *out);
bool lf_song_cache_store_lyrics(const char *title,const char *artist,const lf_lyrics_t *lyrics);
bool lf_song_cache_load_profile(const char *title,const char *artist,
                                int32_t *offset_ms,uint16_t *rate_percent);
bool lf_song_cache_store_profile(const char *title,const char *artist,
                                 int32_t offset_ms,uint16_t rate_percent);
