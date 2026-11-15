#include "spotify.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    /* Client IDs are 32 hex characters. */
    assert(lf_spotify_client_id_valid("0123456789abcdef0123456789ABCDEF"));
    assert(!lf_spotify_client_id_valid("0123456789abcdef0123456789abcde"));
    assert(!lf_spotify_client_id_valid("0123456789abcdef0123456789abcdeg"));
    assert(!lf_spotify_client_id_valid(""));
    assert(!lf_spotify_client_id_valid(NULL));

    /* The one-time code may only be forwarded to a home-network address. */
    assert(lf_spotify_private_ipv4("192.168.0.111"));
    assert(lf_spotify_private_ipv4("10.0.0.5"));
    assert(lf_spotify_private_ipv4("172.16.4.2"));
    assert(lf_spotify_private_ipv4("172.31.255.255"));
    assert(!lf_spotify_private_ipv4("172.32.0.1"));
    assert(!lf_spotify_private_ipv4("8.8.8.8"));
    assert(!lf_spotify_private_ipv4("192.168.0.111/evil"));
    assert(!lf_spotify_private_ipv4("192.168.0"));
    assert(!lf_spotify_private_ipv4("192.168.0.256"));

    char state[48];
    assert(lf_spotify_make_state(state, sizeof state, "0123456789abcdef", "192.168.0.111"));
    assert(strcmp(state, "0123456789abcdef.192.168.0.111") == 0);
    assert(!lf_spotify_make_state(state, sizeof state, "0123456789abcdef", "8.8.8.8"));
    assert(!lf_spotify_make_state(state, sizeof state, "short", "192.168.0.1"));
    assert(!lf_spotify_make_state(state, 10, "0123456789abcdef", "192.168.0.1"));

    /* PKCE challenge encoding: base64url, no padding. */
    char b64[] = "ab+c/d==";
    lf_spotify_base64url(b64);
    assert(strcmp(b64, "ab-c_d") == 0);

    puts("PASS: Spotify client ID, home-network-only callback state and base64url");
    return 0;
}
