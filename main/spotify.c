#include "spotify.h"
#include "lrclib_match.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs.h"
#include "cJSON.h"
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "LFSpotify";
#define NVS_NS "lf_spotify"
#define TOKEN_URL "https://accounts.spotify.com/api/token"
#define NOW_URL "https://api.spotify.com/v1/me/player/currently-playing"
#define NOW_JSON_MAX (32768U)
#define TOKEN_JSON_MAX (4096U)
#define LINK_VALID_MS (15U * 60U * 1000U)

static SemaphoreHandle_t lock;
static char access_token[512];
static uint64_t access_expires_ms;
/* Pending link: PKCE verifier and state, kept in RAM only. */
static char pending_verifier[65];
static char pending_state[48];
static uint64_t pending_since_ms;

static uint64_t now_ms(void) { return (uint64_t)(esp_timer_get_time() / 1000); }
static void take(void)
{
    if (!lock) lock = xSemaphoreCreateMutex();
    if (lock) xSemaphoreTake(lock, portMAX_DELAY);
}
static void give(void) { if (lock) xSemaphoreGive(lock); }
static void wipe(void *p, size_t n)
{
    volatile unsigned char *v = (volatile unsigned char *)p;
    while (n--) *v++ = 0;
}

/* ---------- NVS ---------- */
static bool nvs_get(const char *key, char *out, size_t size)
{
    nvs_handle_t h;
    if (!out || !size || nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t len = size;
    const esp_err_t err = nvs_get_str(h, key, out, &len);
    nvs_close(h);
    if (err != ESP_OK) { out[0] = 0; return false; }
    return out[0] != 0;
}
static bool nvs_put(const char *key, const char *value)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = (value && value[0]) ? nvs_set_str(h, key, value) : nvs_erase_key(h, key);
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

bool lf_spotify_client_id(char *out, size_t size) { return nvs_get("client", out, size); }
bool lf_spotify_save_client_id(const char *id)
{
    if (!id) return false;
    if (id[0] && !lf_spotify_client_id_valid(id)) return false;
    take();
    const bool ok = nvs_put("client", id);
    /* A different app cannot use the old account link. */
    if (ok) { (void)nvs_put("refresh", ""); access_token[0] = 0; access_expires_ms = 0; }
    give();
    return ok;
}
bool lf_spotify_linked(void)
{
    char id[40], token[512];
    const bool linked = lf_spotify_client_id(id, sizeof id) && nvs_get("refresh", token, sizeof token);
    wipe(token, sizeof token);
    return linked;
}
void lf_spotify_unlink(void)
{
    take();
    (void)nvs_put("refresh", "");
    wipe(access_token, sizeof access_token);
    access_expires_ms = 0;
    give();
}

/* ---------- HTTPS ---------- */
/* One request; returns the HTTP status (or -1) and the body in out. */
static int https_call(const char *url, const char *bearer, const char *form,
                      char *out, size_t cap, uint64_t *received_ms)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .method = form ? HTTP_METHOD_POST : HTTP_METHOD_GET,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
        .buffer_size = 4096,
        .buffer_size_tx = 2048,
        .keep_alive_enable = false,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return -1;
    int status = -1;
    if (bearer) {
        char header[540];
        snprintf(header, sizeof header, "Bearer %s", bearer);
        esp_http_client_set_header(c, "Authorization", header);
        wipe(header, sizeof header);
    }
    const int len = form ? (int)strlen(form) : 0;
    if (form) esp_http_client_set_header(c, "Content-Type", "application/x-www-form-urlencoded");
    if (esp_http_client_open(c, len) != ESP_OK) goto done;
    /* The request is on the wire from here; Spotify measures the playback
       position somewhere between now and the reply. */
    const uint64_t sent_ms = now_ms();
    if (len && esp_http_client_write(c, form, len) != len) goto done;
    (void)esp_http_client_fetch_headers(c);
    if (received_ms) *received_ms = sent_ms + (now_ms() - sent_ms) / 2U;
    status = esp_http_client_get_status_code(c);
    if (out && cap) {
        const int n = esp_http_client_read_response(c, out, (int)cap - 1);
        out[n > 0 ? n : 0] = 0;
    }
done:
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    return status;
}

/* Parse a token response; saves a rotated refresh token. Caller holds lock. */
static bool take_tokens(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    const cJSON *access = cJSON_GetObjectItemCaseSensitive(root, "access_token");
    const cJSON *expires = cJSON_GetObjectItemCaseSensitive(root, "expires_in");
    const cJSON *refresh = cJSON_GetObjectItemCaseSensitive(root, "refresh_token");
    bool ok = cJSON_IsString(access) && access->valuestring &&
              strlen(access->valuestring) < sizeof access_token;
    if (ok) {
        strlcpy(access_token, access->valuestring, sizeof access_token);
        const int secs = cJSON_IsNumber(expires) ? expires->valueint : 3600;
        access_expires_ms = now_ms() + (uint64_t)(secs > 120 ? secs - 60 : 60) * 1000U;
        if (cJSON_IsString(refresh) && refresh->valuestring && refresh->valuestring[0])
            ok = nvs_put("refresh", refresh->valuestring);
    }
    if (root) {
        if (cJSON_IsString(access) && access->valuestring) wipe(access->valuestring, strlen(access->valuestring));
        if (cJSON_IsString(refresh) && refresh->valuestring) wipe(refresh->valuestring, strlen(refresh->valuestring));
        cJSON_Delete(root);
    }
    return ok;
}

/* Get a fresh access token from the saved refresh token. Caller holds lock.
   Returns 1 ok, 0 rejected (link again), -1 network. */
static int refresh_access(void)
{
    char id[40], refresh[512], enc[1100];
    char *form = heap_caps_malloc(2048, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    char *body = heap_caps_calloc(1, TOKEN_JSON_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    int result = -1;
    if (!form || !body) goto out;
    if (!lf_spotify_client_id(id, sizeof id) || !nvs_get("refresh", refresh, sizeof refresh)) { result = 0; goto out; }
    if (!lf_lrclib_url_encode(enc, sizeof enc, refresh)) { result = 0; goto out; }
    snprintf(form, 2048, "grant_type=refresh_token&refresh_token=%s&client_id=%s", enc, id);
    const int status = https_call(TOKEN_URL, NULL, form, body, TOKEN_JSON_MAX, NULL);
    if (status == 200 && take_tokens(body)) result = 1;
    else if (status == 400 || status == 401) result = 0;
    if (result != 1) ESP_LOGW(TAG, "Token refresh failed: HTTP %d", status);
out:
    wipe(refresh, sizeof refresh);
    wipe(enc, sizeof enc);
    if (form) { wipe(form, 2048); heap_caps_free(form); }
    if (body) { wipe(body, TOKEN_JSON_MAX); heap_caps_free(body); }
    return result;
}

/* ---------- Linking (PKCE) ---------- */
bool lf_spotify_begin_link(const char *device_ip, char *url, size_t size)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    char id[40];
    if (!url || !lf_spotify_client_id(id, sizeof id)) return false;
    char nonce[17];
    for (int i = 0; i < 8; i++) snprintf(nonce + i * 2, 3, "%02x", (unsigned)(esp_random() & 0xFF));
    char state[48];
    if (!lf_spotify_make_state(state, sizeof state, nonce, device_ip)) return false;
    char verifier[65];
    for (int i = 0; i < 64; i++) verifier[i] = alphabet[esp_random() % (sizeof alphabet - 1)];
    verifier[64] = 0;
    unsigned char digest[32];
    if (mbedtls_sha256((const unsigned char *)verifier, 64, digest, 0) != 0) return false;
    char challenge[64];
    size_t olen = 0;
    if (mbedtls_base64_encode((unsigned char *)challenge, sizeof challenge, &olen, digest, sizeof digest) != 0)
        return false;
    challenge[olen] = 0;
    lf_spotify_base64url(challenge);
    char redirect[160], scope[120], st[96];
    if (!lf_lrclib_url_encode(redirect, sizeof redirect, LF_SPOTIFY_REDIRECT_URI) ||
        !lf_lrclib_url_encode(scope, sizeof scope, LF_SPOTIFY_SCOPES) ||
        !lf_lrclib_url_encode(st, sizeof st, state)) return false;
    const int n = snprintf(url, size,
        "https://accounts.spotify.com/authorize?response_type=code&client_id=%s"
        "&scope=%s&redirect_uri=%s&code_challenge_method=S256&code_challenge=%s&state=%s",
        id, scope, redirect, challenge, st);
    if (n <= 0 || (size_t)n >= size) return false;
    take();
    strlcpy(pending_verifier, verifier, sizeof pending_verifier);
    strlcpy(pending_state, state, sizeof pending_state);
    pending_since_ms = now_ms();
    give();
    wipe(verifier, sizeof verifier);
    return true;
}

bool lf_spotify_finish_link(const char *code, const char *state)
{
    if (!code || !state || !code[0] || strlen(code) > 1024) return false;
    char id[40];
    if (!lf_spotify_client_id(id, sizeof id)) return false;
    take();
    const bool expected = pending_state[0] && strcmp(state, pending_state) == 0 &&
                          now_ms() - pending_since_ms < LINK_VALID_MS;
    if (!expected) { give(); return false; }
    char *form = heap_caps_malloc(4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    char *body = heap_caps_calloc(1, TOKEN_JSON_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    char *enc = heap_caps_malloc(3200, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    char redirect[160];
    bool ok = false;
    if (form && body && enc && lf_lrclib_url_encode(enc, 3200, code) &&
        lf_lrclib_url_encode(redirect, sizeof redirect, LF_SPOTIFY_REDIRECT_URI)) {
        snprintf(form, 4096,
                 "grant_type=authorization_code&code=%s&redirect_uri=%s&client_id=%s&code_verifier=%s",
                 enc, redirect, id, pending_verifier);
        const int status = https_call(TOKEN_URL, NULL, form, body, TOKEN_JSON_MAX, NULL);
        ok = status == 200 && take_tokens(body);
        if (!ok) ESP_LOGW(TAG, "Spotify link failed: HTTP %d", status);
    }
    wipe(pending_verifier, sizeof pending_verifier);
    pending_state[0] = 0;
    if (form) { wipe(form, 4096); heap_caps_free(form); }
    if (body) { wipe(body, TOKEN_JSON_MAX); heap_caps_free(body); }
    if (enc) { wipe(enc, 3200); heap_caps_free(enc); }
    give();
    return ok;
}

/* ---------- Currently playing ---------- */
static lf_spotify_result_t parse_now(const char *json, lf_spotify_now_t *out)
{
    cJSON *root = cJSON_Parse(json);
    if (!cJSON_IsObject(root)) { cJSON_Delete(root); return LF_SPOTIFY_NOTHING; }
    lf_spotify_result_t r = LF_SPOTIFY_NOTHING;
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "currently_playing_type");
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "item");
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
    const cJSON *artists = cJSON_GetObjectItemCaseSensitive(item, "artists");
    const cJSON *first = cJSON_IsArray(artists) ? cJSON_GetArrayItem(artists, 0) : NULL;
    const cJSON *artist = cJSON_GetObjectItemCaseSensitive(first, "name");
    const cJSON *progress = cJSON_GetObjectItemCaseSensitive(root, "progress_ms");
    const cJSON *duration = cJSON_GetObjectItemCaseSensitive(item, "duration_ms");
    const cJSON *playing = cJSON_GetObjectItemCaseSensitive(root, "is_playing");
    if ((!cJSON_IsString(type) || strcmp(type->valuestring, "track") == 0) &&
        cJSON_IsString(name) && name->valuestring[0] &&
        cJSON_IsString(artist) && artist->valuestring[0]) {
        strlcpy(out->title, name->valuestring, sizeof out->title);
        strlcpy(out->artist, artist->valuestring, sizeof out->artist);
        out->progress_ms = cJSON_IsNumber(progress) && progress->valuedouble > 0 ? (uint32_t)progress->valuedouble : 0;
        out->duration_ms = cJSON_IsNumber(duration) && duration->valuedouble > 0 ? (uint32_t)duration->valuedouble : 0;
        out->playing = cJSON_IsTrue(playing);
        r = LF_SPOTIFY_OK;
    }
    cJSON_Delete(root);
    return r;
}

lf_spotify_result_t lf_spotify_now_playing(lf_spotify_now_t *out)
{
    if (!out) return LF_SPOTIFY_NOTHING;
    memset(out, 0, sizeof *out);
    if (!lf_spotify_linked()) return LF_SPOTIFY_NOT_LINKED;
    char *body = heap_caps_calloc(1, NOW_JSON_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!body) return LF_SPOTIFY_NETWORK;
    lf_spotify_result_t result = LF_SPOTIFY_NETWORK;
    take();
    for (int attempt = 0; attempt < 2; attempt++) {
        if (!access_token[0] || now_ms() >= access_expires_ms) {
            const int r = refresh_access();
            if (r == 0) { result = LF_SPOTIFY_AUTH_FAILED; break; }
            if (r < 0) { result = LF_SPOTIFY_NETWORK; break; }
        }
        uint64_t received = 0;
        const int status = https_call(NOW_URL, access_token, NULL, body, NOW_JSON_MAX, &received);
        if (status == 401 && attempt == 0) { access_token[0] = 0; continue; }
        if (status == 200) { result = parse_now(body, out); out->received_ms = received; }
        else if (status == 204) result = LF_SPOTIFY_NOTHING;
        else if (status == 401) result = LF_SPOTIFY_AUTH_FAILED;
        else if (status == 403) result = LF_SPOTIFY_FORBIDDEN;
        else result = LF_SPOTIFY_NETWORK;
        if (status != 200 && status != 204) ESP_LOGW(TAG, "currently-playing: HTTP %d", status);
        break;
    }
    give();
    heap_caps_free(body);
    return result;
}

const char *lf_spotify_result_text(lf_spotify_result_t r)
{
    switch (r) {
    case LF_SPOTIFY_OK: return "Playing on Spotify";
    case LF_SPOTIFY_NOTHING: return "Nothing playing on Spotify";
    case LF_SPOTIFY_NOT_LINKED: return "Spotify not linked";
    case LF_SPOTIFY_AUTH_FAILED: return "Spotify link expired - link again";
    case LF_SPOTIFY_FORBIDDEN: return "Spotify refused (Premium needed)";
    default: return "Spotify unreachable";
    }
}
