#include "spotify.h"
#include <stdio.h>
#include <string.h>

/* Pure helpers for the Spotify link flow; no ESP-IDF dependencies. */

bool lf_spotify_client_id_valid(const char *id)
{
    if (!id || strlen(id) != 32) return false;
    for (const char *c = id; *c; c++)
        if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'f') || (*c >= 'A' && *c <= 'F')))
            return false;
    return true;
}

/* Only home-network addresses: the callback page will only forward the
   one-time code to these, never to a public host. */
bool lf_spotify_private_ipv4(const char *ip)
{
    unsigned a, b, c, d;
    char tail;
    if (!ip || sscanf(ip, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4) return false;
    if (a > 255 || b > 255 || c > 255 || d > 255) return false;
    return a == 10 || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168);
}

bool lf_spotify_make_state(char *out, size_t size, const char *nonce_hex, const char *ip)
{
    if (!out || !nonce_hex || strlen(nonce_hex) != 16 || !lf_spotify_private_ipv4(ip)) return false;
    for (const char *c = nonce_hex; *c; c++)
        if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'f'))) return false;
    const int n = snprintf(out, size, "%s.%s", nonce_hex, ip);
    return n > 0 && (size_t)n < size;
}

void lf_spotify_base64url(char *s)
{
    if (!s) return;
    char *w = s;
    for (char *r = s; *r; r++) {
        if (*r == '=') continue;
        *w++ = *r == '+' ? '-' : *r == '/' ? '_' : *r;
    }
    *w = 0;
}
