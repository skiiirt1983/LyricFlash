#include "ota_pull.h"
#include "network.h"
#include "image_policy.h"
#include "diagnostics.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "esp_system.h"
#include "mbedtls/sha256.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define LF_FEED_URL "https://skiiirt1983.github.io/LyricFlash/beta/updates.json"
#define LF_IMAGE_PREFIX "https://skiiirt1983.github.io/LyricFlash/beta/firmware/"
#define LF_FEED_LIMIT 8192
static const char *TAG = "LyricFlashPull";
static portMUX_TYPE state_mux = portMUX_INITIALIZER_UNLOCKED;
static lf_ota_catalog_t catalog;
static lf_ota_pull_status_t progress = {.state="idle"};
static bool transfer_active;
static TaskHandle_t transfer_task;

typedef struct { lf_ota_entry_t selected; } lf_job_t;

static void status_set(const char *state, const char *detail, uint32_t current, uint32_t total, bool active)
{
    portENTER_CRITICAL(&state_mux);
    snprintf(progress.state, sizeof progress.state, "%s", state);
    snprintf(progress.detail, sizeof progress.detail, "%s", detail ? detail : "");
    progress.received = current;
    progress.total = total;
    progress.active = active;
    portEXIT_CRITICAL(&state_mux);
}

void lf_ota_pull_snapshot(lf_ota_pull_status_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&state_mux);
    *out = progress;
    portEXIT_CRITICAL(&state_mux);
}

void lf_ota_catalog_cached(lf_ota_catalog_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&state_mux);
    *out = catalog;
    portEXIT_CRITICAL(&state_mux);
}

static bool sha_is_hex(const char *s)
{
    if (!s || strlen(s) != 64) return false;
    for (size_t i = 0; i < 64; i++)
        if (!isxdigit((unsigned char)s[i])) return false;
    return true;
}

static bool image_url_allowed(const char *url)
{
    return url && strncmp(url, LF_IMAGE_PREFIX, strlen(LF_IMAGE_PREFIX)) == 0 &&
           strlen(url) < sizeof(((lf_ota_entry_t *)0)->url) &&
           strchr(url + strlen(LF_IMAGE_PREFIX), '?') == NULL &&
           strchr(url + strlen(LF_IMAGE_PREFIX), '#') == NULL &&
           strstr(url + strlen(LF_IMAGE_PREFIX), "..") == NULL;
}

static bool parse_catalog(const char *json, lf_ota_catalog_t *parsed)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;
    const cJSON *schema=cJSON_GetObjectItemCaseSensitive(root, "schema");
    const cJSON *channel=cJSON_GetObjectItemCaseSensitive(root, "channel");
    const cJSON *versions=cJSON_GetObjectItemCaseSensitive(root, "versions");
    bool valid=cJSON_IsNumber(schema) && schema->valueint == 1 &&
        cJSON_IsString(channel) && strcmp(channel->valuestring,"private-beta")==0 &&
        cJSON_IsArray(versions);
    if (!valid) { cJSON_Delete(root); return false; }
    memset(parsed, 0, sizeof *parsed);
    const cJSON *item;
    cJSON_ArrayForEach(item, versions) {
        if (parsed->count >= LF_OTA_CATALOG_MAX) { valid=false; break; }
        const cJSON *id=cJSON_GetObjectItemCaseSensitive(item,"id");
        const cJSON *label=cJSON_GetObjectItemCaseSensitive(item,"label");
        const cJSON *enabled=cJSON_GetObjectItemCaseSensitive(item,"ota");
        if (!cJSON_IsString(id) || !cJSON_IsString(label) || !cJSON_IsBool(enabled) ||
            !id->valuestring || !label->valuestring ||
            strlen(id->valuestring) >= 32 || strlen(label->valuestring) >= 80) {
            valid=false;break;
        }
        lf_ota_entry_t *e=&parsed->entries[parsed->count++];
        snprintf(e->id,sizeof e->id,"%s",id->valuestring);
        snprintf(e->label,sizeof e->label,"%s",label->valuestring);
        e->ota=cJSON_IsTrue(enabled);
        if (!e->ota) continue; /* Factory restore remains USB-only. */
        const cJSON *url=cJSON_GetObjectItemCaseSensitive(item,"url");
        const cJSON *sha=cJSON_GetObjectItemCaseSensitive(item,"sha256");
        const cJSON *bytes=cJSON_GetObjectItemCaseSensitive(item,"bytes");
        if (!cJSON_IsString(url) || !cJSON_IsString(sha) || !cJSON_IsNumber(bytes) ||
            !image_url_allowed(url->valuestring) || !sha_is_hex(sha->valuestring) ||
            bytes->valuedouble != (double)bytes->valueint ||
            bytes->valueint < 512 || bytes->valueint > 0x500000) {
            valid=false;break;
        }
        snprintf(e->url,sizeof e->url,"%s",url->valuestring);
        snprintf(e->sha256,sizeof e->sha256,"%s",sha->valuestring);
        e->bytes=(uint32_t)bytes->valueint;
    }
    cJSON_Delete(root);
    if (!valid || parsed->count < 1 || parsed->entries[parsed->count-1].ota) {
        /* Keep malformed catalogs away from installation code. */
        return false;
    }
    parsed->loaded=true;
    return true;
}

static esp_http_client_handle_t get_client(const char *url)
{
    esp_http_client_config_t cfg={
        .url=url,
        .crt_bundle_attach=esp_crt_bundle_attach,
        .timeout_ms=16000,
        .buffer_size=2048,
        .disable_auto_redirect=true,
        .keep_alive_enable=false,
    };
    return esp_http_client_init(&cfg);
}

bool lf_ota_catalog_refresh(lf_ota_catalog_t *out)
{
    if (!lf_network_is_connected()) {
        if (out) { lf_ota_catalog_cached(out); snprintf(out->error,sizeof out->error,"Device not connected to Wi-Fi"); }
        return false;
    }
    portENTER_CRITICAL(&state_mux);
    bool busy=transfer_active;
    portEXIT_CRITICAL(&state_mux);
    if (busy) { if (out) lf_ota_catalog_cached(out); return false; }

    esp_http_client_handle_t client=get_client(LF_FEED_URL);
    if (!client) return false;
    char *buffer=calloc(LF_FEED_LIMIT+1,1);
    if (!buffer) {esp_http_client_cleanup(client);return false;}
    bool ok=false;
    if (esp_http_client_open(client,0)==ESP_OK) {
        int content_len=esp_http_client_fetch_headers(client);
        int status=esp_http_client_get_status_code(client);
        if (status==200 && content_len>1 && content_len<=LF_FEED_LIMIT) {
            int read_total=0, timeouts=0;
            while (read_total<content_len) {
                int n=esp_http_client_read(client,buffer+read_total,content_len-read_total);
                if (n<=0) {if(++timeouts>3)break;continue;}
                timeouts=0;read_total+=n;
            }
            if (read_total==content_len) {
                lf_ota_catalog_t candidate={0};
                if (parse_catalog(buffer,&candidate)) {
                    portENTER_CRITICAL(&state_mux);
                    catalog=candidate;
                    portEXIT_CRITICAL(&state_mux);
                    ok=true;
                }
            }
        }
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(buffer);
    if(out)lf_ota_catalog_cached(out);
    if(!ok && out)snprintf(out->error,sizeof out->error,"Firmware catalog download/validation failed");
    return ok;
}

static bool digest_matches(const unsigned char digest[32], const char *hex)
{
    static const char lookup[]="0123456789abcdef";
    for (size_t i=0;i<32;i++) {
        if (tolower((unsigned char)hex[2*i]) != lookup[digest[i]>>4] ||
            tolower((unsigned char)hex[2*i+1]) != lookup[digest[i]&15]) return false;
    }
    return true;
}

static void transfer(void *arg)
{
    lf_job_t *job=(lf_job_t *)arg;
    const lf_ota_entry_t entry=job->selected;
    free(job);
    esp_http_client_handle_t client=NULL;
    esp_ota_handle_t ota=0;
    bool started=false;
    bool ok=false;
    uint32_t received=0;
    const char *failure="download failed";
    const esp_partition_t *target=esp_ota_get_next_update_partition(NULL);
    if(!target || entry.bytes > target->size) {failure="No compatible inactive OTA slot";goto finish;}
    client=get_client(entry.url);
    if(!client){failure="HTTPS client unavailable";goto finish;}
    status_set("connecting","Fetching firmware directly from beta server",0,entry.bytes,true);
    if(esp_http_client_open(client,0)!=ESP_OK){failure="HTTPS connection failed";goto finish;}
    int reported=esp_http_client_fetch_headers(client);
    if(esp_http_client_get_status_code(client)!=200 || reported != (int)entry.bytes){
        failure="Unexpected HTTP status or firmware size";goto finish;
    }
    uint8_t first[0x120]={0};
    size_t first_len=0;
    uint8_t chunk[2048];
    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha,0);
    status_set("downloading","Downloading and validating app image",0,entry.bytes,true);
    int stalls=0;
    while(received<entry.bytes){
        int remaining=(int)(entry.bytes-received);
        int n=esp_http_client_read(client,(char *)chunk,
                                   remaining>(int)sizeof(chunk)?sizeof(chunk):remaining);
        if(n<=0){
            if (++stalls>5) {failure="Download stalled/interrupted";break;}
            continue;
        }
        stalls=0;
        mbedtls_sha256_update(&sha,chunk,(size_t)n);
        if(!started){
            size_t copy=(size_t)n;
            if(copy>sizeof(first)-first_len)copy=sizeof(first)-first_len;
            memcpy(first+first_len,chunk,copy);
            first_len+=copy;
            if(first_len==sizeof(first)){
                if(lf_check_ota_image(first,sizeof(first),target->size)!=LF_IMAGE_OK) {
                    failure="Wrong device or invalid app image";break;
                }
                if(esp_ota_begin(target,entry.bytes,&ota)!=ESP_OK) {
                    failure="Cannot begin OTA flash";break;
                }
                started=true;
                if(esp_ota_write(ota,first,first_len)!=ESP_OK ||
                   ((size_t)n>copy && esp_ota_write(ota,chunk+copy,n-copy)!=ESP_OK)) {
                    failure="Initial flash write failed";break;
                }
            }
        } else if(esp_ota_write(ota,chunk,n)!=ESP_OK) {
            failure="Flash write failed";break;
        }
        received+=(uint32_t)n;
        status_set("downloading","Downloading to inactive OTA slot",received,entry.bytes,true);
    }
    if(received==entry.bytes && started){
        unsigned char digest[32];
        mbedtls_sha256_finish(&sha,digest);
        if(!digest_matches(digest,entry.sha256))failure="SHA-256 mismatch: update rejected";
        else {
            status_set("verifying","Full SHA-256 verified; checking app signature",received,entry.bytes,true);
            esp_err_t err=esp_ota_end(ota);
            started=false;
            if(err!=ESP_OK)failure="ESP32 image validation failed";
            else if(esp_ota_set_boot_partition(target)!=ESP_OK)failure="Cannot activate update";
            else ok=true;
        }
    }
    mbedtls_sha256_free(&sha);
finish:
    if (started) esp_ota_abort(ota);
    if (client) { esp_http_client_close(client); esp_http_client_cleanup(client); }
    if(ok){
        status_set("rebooting","Firmware verified; restarting into selected version",received,entry.bytes,false);
        ESP_LOGI(TAG,"Firmware %s installed; rebooting",entry.id);
        vTaskDelay(pdMS_TO_TICKS(1700));
        esp_restart();
    }else{
        lf_diag_error("ota_pull",failure);
        ESP_LOGE(TAG,"Update %s failed: %s (%lu/%lu)",entry.id,failure,
                 (unsigned long)received,(unsigned long)entry.bytes);
        status_set("failed",failure,received,entry.bytes,false);
    }
    portENTER_CRITICAL(&state_mux);
    transfer_active=false;
    transfer_task=NULL;
    portEXIT_CRITICAL(&state_mux);
    vTaskDelete(NULL);
}

bool lf_ota_pull_start(const char *version_id)
{
    if (!version_id || !lf_network_is_connected()) return false;
    lf_job_t *job=calloc(1,sizeof(*job));
    if (!job) return false;
    portENTER_CRITICAL(&state_mux);
    bool allowed=!transfer_active && catalog.loaded;
    if(allowed) {
        allowed=false;
        for(size_t i=0;i<catalog.count;i++){
            if(catalog.entries[i].ota && strcmp(catalog.entries[i].id,version_id)==0) {
                job->selected=catalog.entries[i];
                allowed=true;break;
            }
        }
    }
    if(allowed){
        transfer_active=true;
        snprintf(progress.selected,sizeof progress.selected,"%s",version_id);
        snprintf(progress.state,sizeof progress.state,"queued");
        progress.active=true;
        progress.received=0;progress.total=job->selected.bytes;
    }
    portEXIT_CRITICAL(&state_mux);
    if(!allowed){free(job);return false;}
    if(xTaskCreate(transfer,"lf_pull_ota",10240,job,4,&transfer_task)!=pdPASS){
        free(job);
        status_set("failed","Unable to create OTA worker",0,0,false);
        portENTER_CRITICAL(&state_mux);transfer_active=false;transfer_task=NULL;portEXIT_CRITICAL(&state_mux);
        return false;
    }
    return true;
}
