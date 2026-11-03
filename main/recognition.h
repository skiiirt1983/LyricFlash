#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "lyrics.h"

/* One explicit scan at a time. No background listening, no raw audio APIs. */
typedef enum {
    LF_RECOG_IDLE=0, LF_RECOG_CAPTURE, LF_RECOG_UPLOAD, LF_RECOG_LYRICS, LF_RECOG_MATCH,
    LF_RECOG_NO_MATCH, LF_RECOG_ERROR, LF_RECOG_STOPPED
} lf_recog_state_t;
typedef struct {
    lf_recog_state_t state;
    uint32_t captured_frames;
    uint32_t capture_seconds;
    uint32_t attempts;
    int http_status;
    char title[128];
    char artist[128];
    char message[128];
    char lyrics_state[24]; /* synced, plain_only, not_found, etc. */
    uint32_t lyric_lines;
} lf_recog_snapshot_t;

bool lf_recog_has_token(void);
bool lf_recog_save_token(const char *value);
bool lf_recog_clear_token(void);
bool lf_recog_start(void);
void lf_recog_stop(void);
bool lf_recog_busy(void);
void lf_recog_snapshot(lf_recog_snapshot_t *out);
const char *lf_recog_state_name(lf_recog_state_t state);

bool lf_recog_lyric_line(uint32_t index, lf_lyric_line_t *out);
