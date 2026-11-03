#include "lrclib_client.h"
#include "lrclib_match.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

/* No CDN, proxy or lyrics uploads. LRCLIB's public API requires an identified
 * user-agent; requests are HTTPS and sequential; 404 may use ONE title fallback
 * after a 300ms delay. 429 is never automatically retried. */
#define LF_LRCLIB_JSON_MAX (65536U)
#define LF_LRCLIB_URL_MAX (920U)
static const char *const client_user_agent=
    "LyricFlash/0.8.38 (https://github.com/skiiirt1983/LyricFlash)";
const char *lf_lrclib_state_name(lf_lrclib_state_t s)
{
    switch(s) {
    case LF_LRCLIB_SYNCED:return "synced";
    case LF_LRCLIB_PLAIN:return "plain_only";
    case LF_LRCLIB_INSTRUMENTAL:return "instrumental";
    case LF_LRCLIB_NOT_FOUND:return "not_found";
    case LF_LRCLIB_RATE_LIMIT:return "rate_limited";
    case LF_LRCLIB_UNAVAILABLE:return "unavailable";
    case LF_LRCLIB_CANCELLED:return "cancelled";
    default:return "unavailable";
    }
}
static bool build_url(char url[LF_LRCLIB_URL_MAX],const char *track,const char *artist)
{
    char title[384],name[384];
    if(!lf_lrclib_url_encode(title,sizeof title,track) ||
       !lf_lrclib_url_encode(name,sizeof name,artist))return false;
    const int n=snprintf(url,LF_LRCLIB_URL_MAX,
              "https://lrclib.net/api/get?track_name=%s&artist_name=%s",
              title,name);
    return n>0 && (size_t)n<LF_LRCLIB_URL_MAX;
}
static lf_lrclib_state_t request_one(const char *track,const char *artist,
                                  lf_lyrics_t *out,bool(*cancelled)(void))
{
    char url[LF_LRCLIB_URL_MAX];
    if(!build_url(url,track,artist))return LF_LRCLIB_UNAVAILABLE;
    if(cancelled())return LF_LRCLIB_CANCELLED;
    esp_http_client_config_t cfg={
        .url=url,
        .crt_bundle_attach=esp_crt_bundle_attach,
        .user_agent=client_user_agent,
        .timeout_ms=12000,
        .buffer_size=4096,
        .keep_alive_enable=false
    };
    esp_http_client_handle_t client=esp_http_client_init(&cfg);
    if(!client)return LF_LRCLIB_UNAVAILABLE;
    lf_lrclib_state_t result=LF_LRCLIB_UNAVAILABLE;
    if(esp_http_client_open(client,0)!=ESP_OK)goto done;
    const int64_t length=esp_http_client_fetch_headers(client);
    const int status=esp_http_client_get_status_code(client);
    if(cancelled()){result=LF_LRCLIB_CANCELLED;goto done;}
    if(status==404){result=LF_LRCLIB_NOT_FOUND;goto done;}
    if(status==429){result=LF_LRCLIB_RATE_LIMIT;goto done;}
    if(status!=200||length>=LF_LRCLIB_JSON_MAX)goto done;
    char *response=(char *)heap_caps_calloc(1,LF_LRCLIB_JSON_MAX,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!response)goto done;
    const int bytes=esp_http_client_read_response(client,response,LF_LRCLIB_JSON_MAX-1U);
    if(bytes<=0||bytes>=(int)LF_LRCLIB_JSON_MAX-1 || cancelled()){
        result=cancelled()?LF_LRCLIB_CANCELLED:LF_LRCLIB_UNAVAILABLE;
        goto free_response;
    }
    response[bytes]=0;
    cJSON *root=cJSON_Parse(response);
    if(!cJSON_IsObject(root)){cJSON_Delete(root);goto free_response;}
    const cJSON *instrumental=cJSON_GetObjectItemCaseSensitive(root,"instrumental");
    const cJSON *synced=cJSON_GetObjectItemCaseSensitive(root,"syncedLyrics");
    const cJSON *plain=cJSON_GetObjectItemCaseSensitive(root,"plainLyrics");
    if(cJSON_IsTrue(instrumental)){
        result=LF_LRCLIB_INSTRUMENTAL;
    }else if(cJSON_IsString(synced)&&synced->valuestring&&synced->valuestring[0]){
        /* Built-in LRC parser sorts real timestamps and rejects bad lines. */
        result=lf_parse_lrc(synced->valuestring,out)>0?
                 LF_LRCLIB_SYNCED:LF_LRCLIB_UNAVAILABLE;
    }else if(cJSON_IsString(plain)&&plain->valuestring&&plain->valuestring[0]){
        result=LF_LRCLIB_PLAIN;
    }else{
        result=LF_LRCLIB_NOT_FOUND;
    }
    cJSON_Delete(root);
free_response:
    memset(response,0,(size_t)bytes>0&&(size_t)bytes<LF_LRCLIB_JSON_MAX?(size_t)bytes:1U);
    heap_caps_free(response);
done:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return result;
}
lf_lrclib_state_t lf_lrclib_lookup(const char *track,const char *artist,
                                   lf_lyrics_t *out,bool(*cancelled)(void))
{
    if(!track||!artist||!out||!cancelled||!track[0]||!artist[0])
        return LF_LRCLIB_UNAVAILABLE;
    out->count=0;
    lf_lrclib_state_t result=request_one(track,artist,out,cancelled);
    if(result!=LF_LRCLIB_NOT_FOUND || cancelled())return result;
    /* Only a verified suffix is dropped; e.g. Single Version → base track.
     * Do not broadly search and accidentally attribute other artists' lyrics. */
    char fallback[128];
    if(!lf_lrclib_search_title(fallback,sizeof fallback,track))
        return LF_LRCLIB_NOT_FOUND;
    vTaskDelay(pdMS_TO_TICKS(300)); /* LRCLIB's recommended polite spacing */
    if(cancelled())return LF_LRCLIB_CANCELLED;
    return request_one(fallback,artist,out,cancelled);
}
