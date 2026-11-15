#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Spotify "currently playing" as a Song ID source (1.1.0).
 *
 * The owner creates a Spotify developer app and enters its Client ID on the
 * device's web page. Linking uses Authorization Code with PKCE (no client
 * secret). Spotify only redirects to HTTPS, so the redirect URI is a small
 * page on the LyricFlash site that forwards the one-time code to the device
 * on the local network (the device IP travels in the OAuth state). */

#define LF_SPOTIFY_REDIRECT_URI "https://skiiirt1983.github.io/LyricFlash/spotify/"
#define LF_SPOTIFY_SCOPES "user-read-currently-playing user-read-playback-state"

typedef enum {
    LF_SPOTIFY_OK = 0,        /* a track is playing (or paused) */
    LF_SPOTIFY_NOTHING,       /* nothing playing, or not a music track */
    LF_SPOTIFY_NOT_LINKED,    /* no Client ID or no linked account */
    LF_SPOTIFY_AUTH_FAILED,   /* refresh token rejected: link again */
    LF_SPOTIFY_FORBIDDEN,     /* 403: Premium / developer access missing */
    LF_SPOTIFY_NETWORK,       /* could not reach Spotify */
} lf_spotify_result_t;

typedef struct {
    char title[128];
    char artist[128];
    uint32_t progress_ms;
    uint32_t duration_ms;
    bool playing;
    uint64_t received_ms; /* monotonic ms when progress_ms was received */
} lf_spotify_now_t;

bool lf_spotify_client_id(char *out, size_t size);   /* false if none saved */
bool lf_spotify_save_client_id(const char *client_id); /* "" clears it */
bool lf_spotify_linked(void);                          /* refresh token saved */
void lf_spotify_unlink(void);

/* Start linking: builds the Spotify authorize URL for this device. */
bool lf_spotify_begin_link(const char *device_ip, char *url, size_t size);
/* Finish linking with the code Spotify returned; false on any failure. */
bool lf_spotify_finish_link(const char *code, const char *state);

lf_spotify_result_t lf_spotify_now_playing(lf_spotify_now_t *out);
const char *lf_spotify_result_text(lf_spotify_result_t r);

/* Pure helpers (host tested in tests/test_spotify_logic.c). */
bool lf_spotify_client_id_valid(const char *id);
/* state = "<16 hex nonce>.<IPv4>"; the callback page reads the IP from it. */
bool lf_spotify_make_state(char *out, size_t size, const char *nonce_hex, const char *ip);
bool lf_spotify_private_ipv4(const char *ip);
/* Standard base64 -> base64url without padding, in place. */
void lf_spotify_base64url(char *s);
