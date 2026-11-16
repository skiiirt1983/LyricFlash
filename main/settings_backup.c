#include "settings_backup.h"
#include "settings.h"
#include "version.h"
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "mbedtls/base64.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "LFBackup";
#define BACKUP_FORMAT "lyricflash-settings"
#define STORE_LABEL "sounds"
#define STORE_MOUNT "/lfstore"
#define STORE_FILE STORE_MOUNT "/lyricflash-settings.json"
#define BACKUP_MAX (96U * 1024U)

/* Only these namespaces are ever read or written. */
static const char *const namespaces[] = {
    LF_CONFIG_NS,   /* display, lyrics and clock settings */
    "lf_network",   /* home Wi-Fi */
    "lf_weather",   /* weather city and interval */
    "lf_recog",     /* AudD key */
    "lf_spotify",   /* Spotify Client ID and link */
};
#define NS_COUNT (sizeof namespaces / sizeof namespaces[0])

static const char *type_name(nvs_type_t t)
{
    switch (t) {
    case NVS_TYPE_U8: return "u8";   case NVS_TYPE_I8: return "i8";
    case NVS_TYPE_U16: return "u16"; case NVS_TYPE_I16: return "i16";
    case NVS_TYPE_U32: return "u32"; case NVS_TYPE_I32: return "i32";
    case NVS_TYPE_U64: return "u64"; case NVS_TYPE_I64: return "i64";
    case NVS_TYPE_STR: return "str"; case NVS_TYPE_BLOB: return "blob";
    default: return NULL;
    }
}

static cJSON *entry_json(nvs_handle_t h, const nvs_entry_info_t *info)
{
    const char *t = type_name(info->type);
    if (!t) return NULL;
    cJSON *e = cJSON_CreateObject();
    if (!e) return NULL;
    cJSON_AddStringToObject(e, "t", t);
    bool ok = false;
    char text[24];
    switch (info->type) {
    case NVS_TYPE_U8:  { uint8_t v;  ok = nvs_get_u8(h, info->key, &v) == ESP_OK;  if (ok) cJSON_AddNumberToObject(e, "v", v); break; }
    case NVS_TYPE_I8:  { int8_t v;   ok = nvs_get_i8(h, info->key, &v) == ESP_OK;  if (ok) cJSON_AddNumberToObject(e, "v", v); break; }
    case NVS_TYPE_U16: { uint16_t v; ok = nvs_get_u16(h, info->key, &v) == ESP_OK; if (ok) cJSON_AddNumberToObject(e, "v", v); break; }
    case NVS_TYPE_I16: { int16_t v;  ok = nvs_get_i16(h, info->key, &v) == ESP_OK; if (ok) cJSON_AddNumberToObject(e, "v", v); break; }
    case NVS_TYPE_U32: { uint32_t v; ok = nvs_get_u32(h, info->key, &v) == ESP_OK; if (ok) cJSON_AddNumberToObject(e, "v", v); break; }
    case NVS_TYPE_I32: { int32_t v;  ok = nvs_get_i32(h, info->key, &v) == ESP_OK; if (ok) cJSON_AddNumberToObject(e, "v", v); break; }
    /* 64-bit values travel as text: JSON numbers lose precision. */
    case NVS_TYPE_U64: { uint64_t v; ok = nvs_get_u64(h, info->key, &v) == ESP_OK;
                         if (ok) { snprintf(text, sizeof text, "%" PRIu64, v); cJSON_AddStringToObject(e, "v", text); } break; }
    case NVS_TYPE_I64: { int64_t v;  ok = nvs_get_i64(h, info->key, &v) == ESP_OK;
                         if (ok) { snprintf(text, sizeof text, "%" PRId64, v); cJSON_AddStringToObject(e, "v", text); } break; }
    case NVS_TYPE_STR: {
        size_t len = 0;
        if (nvs_get_str(h, info->key, NULL, &len) != ESP_OK || len == 0 || len > 4096) break;
        char *s = malloc(len);
        if (!s) break;
        ok = nvs_get_str(h, info->key, s, &len) == ESP_OK;
        if (ok) cJSON_AddStringToObject(e, "v", s);
        memset(s, 0, len);
        free(s);
        break;
    }
    case NVS_TYPE_BLOB: {
        size_t len = 0;
        if (nvs_get_blob(h, info->key, NULL, &len) != ESP_OK || len > 4096) break;
        unsigned char *raw = malloc(len ? len : 1);
        const size_t cap = ((len + 2) / 3) * 4 + 1;
        unsigned char *b64 = malloc(cap);
        size_t out = 0;
        if (raw && b64 && nvs_get_blob(h, info->key, raw, &len) == ESP_OK &&
            mbedtls_base64_encode(b64, cap, &out, raw, len) == 0) {
            b64[out] = 0;
            cJSON_AddStringToObject(e, "v", (const char *)b64);
            ok = true;
        }
        free(raw);
        free(b64);
        break;
    }
    default: break;
    }
    if (!ok) { cJSON_Delete(e); return NULL; }
    return e;
}

char *lf_backup_export(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *all = root ? cJSON_AddObjectToObject(root, "settings") : NULL;
    if (!all) { cJSON_Delete(root); return NULL; }
    cJSON_AddStringToObject(root, "format", BACKUP_FORMAT);
    cJSON_AddNumberToObject(root, "schema", 1);
    cJSON_AddStringToObject(root, "firmware", LF_VERSION);
    unsigned count = 0;
    for (size_t n = 0; n < NS_COUNT; n++) {
        nvs_handle_t h;
        if (nvs_open(namespaces[n], NVS_READONLY, &h) != ESP_OK) continue;
        cJSON *ns = cJSON_AddObjectToObject(all, namespaces[n]);
        nvs_iterator_t it = NULL;
        esp_err_t r = nvs_entry_find(NVS_DEFAULT_PART_NAME, namespaces[n], NVS_TYPE_ANY, &it);
        while (ns && r == ESP_OK) {
            nvs_entry_info_t info;
            if (nvs_entry_info(it, &info) == ESP_OK) {
                cJSON *e = entry_json(h, &info);
                if (e) { cJSON_AddItemToObject(ns, info.key, e); count++; }
            }
            r = nvs_entry_next(&it);
        }
        nvs_release_iterator(it);
        nvs_close(h);
    }
    cJSON_AddNumberToObject(root, "count", count);
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return text;
}

static bool allowed_namespace(const char *name)
{
    for (size_t n = 0; n < NS_COUNT; n++)
        if (strcmp(name, namespaces[n]) == 0) return true;
    return false;
}

static bool entry_restore(nvs_handle_t h, const char *key, const cJSON *e)
{
    const cJSON *t = cJSON_GetObjectItemCaseSensitive(e, "t");
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(e, "v");
    if (!cJSON_IsString(t) || !v || strlen(key) == 0 || strlen(key) > 15) return false;
    const char *type = t->valuestring;
    if (cJSON_IsNumber(v)) {
        const double d = v->valuedouble;
        if (!strcmp(type, "u8"))  return nvs_set_u8(h, key, (uint8_t)d) == ESP_OK;
        if (!strcmp(type, "i8"))  return nvs_set_i8(h, key, (int8_t)d) == ESP_OK;
        if (!strcmp(type, "u16")) return nvs_set_u16(h, key, (uint16_t)d) == ESP_OK;
        if (!strcmp(type, "i16")) return nvs_set_i16(h, key, (int16_t)d) == ESP_OK;
        if (!strcmp(type, "u32")) return nvs_set_u32(h, key, (uint32_t)d) == ESP_OK;
        if (!strcmp(type, "i32")) return nvs_set_i32(h, key, (int32_t)d) == ESP_OK;
        return false;
    }
    if (!cJSON_IsString(v) || !v->valuestring) return false;
    const char *s = v->valuestring;
    if (!strcmp(type, "u64")) return nvs_set_u64(h, key, strtoull(s, NULL, 10)) == ESP_OK;
    if (!strcmp(type, "i64")) return nvs_set_i64(h, key, strtoll(s, NULL, 10)) == ESP_OK;
    if (!strcmp(type, "str")) return strlen(s) < 4096 && nvs_set_str(h, key, s) == ESP_OK;
    if (!strcmp(type, "blob")) {
        const size_t cap = strlen(s) / 4 * 3 + 4;
        unsigned char *raw = malloc(cap);
        size_t len = 0;
        const bool ok = raw && mbedtls_base64_decode(raw, cap, &len, (const unsigned char *)s, strlen(s)) == 0 &&
                        nvs_set_blob(h, key, raw, len) == ESP_OK;
        free(raw);
        return ok;
    }
    return false;
}

int lf_backup_import(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    const cJSON *format = cJSON_GetObjectItemCaseSensitive(root, "format");
    const cJSON *all = cJSON_GetObjectItemCaseSensitive(root, "settings");
    if (!cJSON_IsString(format) || strcmp(format->valuestring, BACKUP_FORMAT) != 0 || !cJSON_IsObject(all)) {
        cJSON_Delete(root);
        return -1;
    }
    int restored = 0;
    const cJSON *ns;
    cJSON_ArrayForEach(ns, all) {
        if (!ns->string || !allowed_namespace(ns->string) || !cJSON_IsObject(ns)) continue;
        nvs_handle_t h;
        if (nvs_open(ns->string, NVS_READWRITE, &h) != ESP_OK) continue;
        const cJSON *e;
        cJSON_ArrayForEach(e, ns) {
            if (e->string && cJSON_IsObject(e) && entry_restore(h, e->string, e)) restored++;
        }
        (void)nvs_commit(h);
        nvs_close(h);
    }
    cJSON_Delete(root);
    ESP_LOGI(TAG, "Restored %d settings", restored);
    return restored;
}

/* ---------- on-device copy (LittleFS in the "sounds" partition) ---------- */

/* Another app (PrintSphere) may own a filesystem here. Format only when the
   partition is still blank, never when a mount merely failed. */
static bool store_is_blank(const esp_partition_t *part)
{
    uint8_t block[256];
    for (size_t off = 0; off < 4096; off += sizeof block) {
        if (esp_partition_read(part, off, block, sizeof block) != ESP_OK) return false;
        for (size_t i = 0; i < sizeof block; i++)
            if (block[i] != 0xFF) return false;
    }
    return true;
}

static bool store_mount(void)
{
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, STORE_LABEL);
    if (!part) { ESP_LOGW(TAG, "No '%s' partition for the device backup", STORE_LABEL); return false; }
    esp_vfs_littlefs_conf_t conf = {
        .base_path = STORE_MOUNT,
        .partition_label = STORE_LABEL,
        .format_if_mount_failed = false,
        .dont_mount = false,
    };
    if (esp_vfs_littlefs_register(&conf) == ESP_OK) return true;
    if (!store_is_blank(part)) {
        ESP_LOGW(TAG, "Backup storage holds other data and could not be opened; left untouched");
        return false;
    }
    conf.format_if_mount_failed = true;
    return esp_vfs_littlefs_register(&conf) == ESP_OK;
}
static void store_unmount(void) { (void)esp_vfs_littlefs_unregister(STORE_LABEL); }

static char *store_read(void)
{
    if (!store_mount()) return NULL;
    char *text = NULL;
    FILE *f = fopen(STORE_FILE, "rb");
    if (f) {
        text = heap_caps_calloc(1, BACKUP_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (text) {
            const size_t n = fread(text, 1, BACKUP_MAX - 1, f);
            text[n] = 0;
            if (n == 0) { heap_caps_free(text); text = NULL; }
        }
        fclose(f);
    }
    store_unmount();
    return text;
}

bool lf_backup_device_save(void)
{
    char *text = lf_backup_export();
    if (!text) return false;
    const size_t len = strlen(text);
    bool ok = false;
    if (len < BACKUP_MAX && store_mount()) {
        FILE *f = fopen(STORE_FILE ".new", "wb");
        if (f) {
            ok = fwrite(text, 1, len, f) == len;
            ok = fclose(f) == 0 && ok;
            /* Replace the old copy only after the new one is complete. */
            if (ok) { (void)remove(STORE_FILE); ok = rename(STORE_FILE ".new", STORE_FILE) == 0; }
        }
        store_unmount();
    }
    memset(text, 0, len);
    free(text);
    return ok;
}

int lf_backup_device_restore(void)
{
    char *text = store_read();
    if (!text) return -1;
    const int n = lf_backup_import(text);
    memset(text, 0, strlen(text));
    heap_caps_free(text);
    return n;
}

bool lf_backup_device_info(char *version, size_t size)
{
    if (version && size) version[0] = 0;
    char *text = store_read();
    if (!text) return false;
    cJSON *root = cJSON_Parse(text);
    const cJSON *format = cJSON_GetObjectItemCaseSensitive(root, "format");
    const cJSON *fw = cJSON_GetObjectItemCaseSensitive(root, "firmware");
    const bool ok = cJSON_IsString(format) && strcmp(format->valuestring, BACKUP_FORMAT) == 0;
    if (ok && version && size && cJSON_IsString(fw)) snprintf(version, size, "%s", fw->valuestring);
    cJSON_Delete(root);
    memset(text, 0, strlen(text));
    heap_caps_free(text);
    return ok;
}
