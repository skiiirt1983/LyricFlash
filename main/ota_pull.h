#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LF_OTA_CATALOG_MAX 8

typedef struct {
    char id[32];
    char label[80];
    char url[240];
    char sha256[65];
    uint32_t bytes;
    bool ota;
} lf_ota_entry_t;

typedef struct {
    lf_ota_entry_t entries[LF_OTA_CATALOG_MAX];
    size_t count;
    bool loaded;
    char error[112];
} lf_ota_catalog_t;

typedef struct {
    bool active;
    uint32_t received;
    uint32_t total;
    char state[32];
    char detail[112];
    char selected[32];
} lf_ota_pull_status_t;

/* Network operations happen on the ESP32. No browser firmware upload required. */
bool lf_ota_catalog_refresh(lf_ota_catalog_t *out);
void lf_ota_catalog_cached(lf_ota_catalog_t *out);
bool lf_ota_pull_start(const char *version_id);
void lf_ota_pull_snapshot(lf_ota_pull_status_t *out);
