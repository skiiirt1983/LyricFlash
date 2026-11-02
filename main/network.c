#include "network.h"
#include "ui.h"
#include "settings.h"
#include "weather.h"
#include "diagnostics.h"
#include "version.h"
#include "web_basic_html.h"
#include "web_device_html.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include <stdatomic.h>
#include "cJSON.h"
#include "image_policy.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include <time.h>
#include <stdlib.h>
#include "esp_event.h"
#include "esp_random.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>

static const char *TAG="LFNetwork";
static EventGroupHandle_t connected_event;
#define LF_CONNECTED BIT0
static bool in_ap;
static char address[32] = "Offline";
static char ap_ssid[33];
static char ap_pass[16];
static httpd_handle_t server;
static const char *ota_result = "idle";
static uint32_t ota_received;
static uint32_t ota_total;
static atomic_bool ota_busy = false;
/* Explain OTA HTTPS failures to the browser without leaking Wi-Fi credentials. */
static const char *ota_detail = "";
static atomic_int ota_error_code = 0;
static atomic_int ota_http_status = 0;
static atomic_int ota_tls_code = 0;
static atomic_int ota_tls_flags = 0;
static atomic_int ota_socket_errno = 0;
/* URL is taken only from the official LyricFlash GitHub Pages beta asset
   or versioned Github Release assets belonging to this repository. */
typedef struct {
    char url[384];
    char version[33];
} lf_github_request_t;
static bool time_sync_started;
static void reboot_task(void *arg);

/* Scan completion runs on the ESP event task, UI reads on the LVGL task. */
static portMUX_TYPE wifi_scan_mux = portMUX_INITIALIZER_UNLOCKED;
static lf_wifi_ap_t wifi_scan_list[LF_WIFI_MAX_RESULTS];
static size_t wifi_scan_count;
static int wifi_scan_state; /* 0 idle, 1 running, 2 ready, -1 failed */

static void wifi_scan_done(void)
{
    wifi_ap_record_t records[LF_WIFI_MAX_RESULTS] = {0};
    uint16_t found = LF_WIFI_MAX_RESULTS;
    esp_err_t err = esp_wifi_scan_get_ap_records(&found, records);
    lf_wifi_ap_t entries[LF_WIFI_MAX_RESULTS] = {0};
    size_t count = 0;
    if (err == ESP_OK) {
        for (uint16_t i = 0; i < found && count < LF_WIFI_MAX_RESULTS; i++) {
            if (!records[i].ssid[0]) continue; /* Hide non-broadcast SSIDs. */
            char name[33] = {0};
            memcpy(name, records[i].ssid, 32);
            bool duplicate = false;
            for (size_t j = 0; j < count; j++) {
                if (strcmp(entries[j].ssid, name) == 0) { duplicate = true; break; }
            }
            if (duplicate) continue;
            strlcpy(entries[count].ssid, name, sizeof(entries[count].ssid));
            entries[count].rssi = records[i].rssi;
            entries[count].secured = records[i].authmode != WIFI_AUTH_OPEN;
            count++;
        }
    }
    portENTER_CRITICAL(&wifi_scan_mux);
    wifi_scan_count = count;
    memcpy(wifi_scan_list, entries, sizeof(entries));
    wifi_scan_state = err == ESP_OK ? 2 : -1;
    portEXIT_CRITICAL(&wifi_scan_mux);
}

bool lf_network_scan_start(void)
{
    portENTER_CRITICAL(&wifi_scan_mux);
    if (wifi_scan_state == 1) {
        portEXIT_CRITICAL(&wifi_scan_mux);
        return false;
    }
    wifi_scan_count = 0;
    wifi_scan_state = 1;
    portEXIT_CRITICAL(&wifi_scan_mux);
    /* ESP-IDF cannot scan in AP-only mode. Keep the fallback setup AP
       available while enabling the STA interface for an asynchronous scan. */
    esp_err_t err = ESP_OK;
    if (in_ap) err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err == ESP_OK) err = esp_wifi_scan_start(NULL, false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi scan could not start: %s", esp_err_to_name(err));
        portENTER_CRITICAL(&wifi_scan_mux);
        wifi_scan_state = -1;
        portEXIT_CRITICAL(&wifi_scan_mux);
        return false;
    }
    return true;
}

int lf_network_scan_results(lf_wifi_ap_t *out, size_t max, size_t *count)
{
    if (!count || (max && !out)) return -1;
    portENTER_CRITICAL(&wifi_scan_mux);
    int state = wifi_scan_state;
    size_t copied = wifi_scan_count < max ? wifi_scan_count : max;
    if (out && copied) memcpy(out, wifi_scan_list, copied * sizeof(*out));
    *count = copied;
    portEXIT_CRITICAL(&wifi_scan_mux);
    return state;
}

/* Save selection from the on-device screen, then restart the normal
   STA startup path. Credentials are never printed in ESP logs. */
bool lf_network_save_wifi(const char *ssid, const char *password)
{
    if (!ssid || !password) return false;
    size_t sn = strlen(ssid), pn = strlen(password);
    if (!sn || sn > 32 || pn > 63 || (pn > 0 && pn < 8)) return false;
    nvs_handle_t h;
    if (nvs_open("lf_network", NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t e = nvs_set_str(h, "ssid", ssid);
    if (e == ESP_OK) e = nvs_set_str(h, "password", password);
    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);
    if (e != ESP_OK) return false;
    return true;
}

void lf_network_restart(void)
{
    xTaskCreate(reboot_task, "lf_wifi_reboot", 2048, NULL, 3, NULL);
}

bool lf_network_is_connected(void)
{
    return !in_ap && connected_event &&
           (xEventGroupGetBits(connected_event) & LF_CONNECTED);
}

/* Pacific Time is the Alpha default. Later versions can expose time zone
   as a user setting. NTP needs Wi-Fi but does not require a cloud account. */
static void start_time_sync(void)
{
    /* British Columbia (most regions) has used permanent UTC-7 PCT since 2026.
       POSIX TZ signs are reversed: PCT7 means UTC minus seven hours. */
    setenv("TZ", "PCT7", 1);
    tzset();
    if (time_sync_started) return;
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_err_t err = esp_netif_sntp_init(&config);
    if (err == ESP_OK) time_sync_started = true;
    else ESP_LOGW(TAG, "Time sync unavailable: %s", esp_err_to_name(err));
}
bool lf_network_connected_ssid(char *out, size_t size)
{
    if (!out || !size) return false;
    out[0] = 0;
    if (!lf_network_is_connected()) return false;
    wifi_ap_record_t ap = {0};
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return false;
    snprintf(out, size, "%.32s", (const char *)ap.ssid);
    return out[0] != 0;
}
static bool nvs_wifi_load(char *ssid, size_t ns, char *pass, size_t np)
{
    nvs_handle_t h;
    if(nvs_open("lf_network", NVS_READONLY, &h) != ESP_OK) return false;
    size_t a=ns,b=np;
    esp_err_t e=nvs_get_str(h,"ssid",ssid,&a);
    esp_err_t p=nvs_get_str(h,"password",pass,&b);
    nvs_close(h);
    return e==ESP_OK && p==ESP_OK && ssid[0];
}
static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if(base==WIFI_EVENT && id==WIFI_EVENT_SCAN_DONE) wifi_scan_done();
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED && !in_ap) {
        xEventGroupClearBits(connected_event, LF_CONNECTED);
        strlcpy(address, "Offline", sizeof(address));
        esp_wifi_connect();
    }
    if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *got=(const ip_event_got_ip_t *)data;
        snprintf(address,sizeof(address),IPSTR,IP2STR(&got->ip_info.ip));
        xEventGroupSetBits(connected_event, LF_CONNECTED);
    }
}
static void reboot_task(void *arg)
{
    (void)arg; vTaskDelay(pdMS_TO_TICKS(900)); esp_restart();
}
static esp_err_t handle_home(httpd_req_t *req)
{
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_send(req,lf_device_settings_html,HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_firmware_page(httpd_req_t *req)
{
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_send(req,lf_device_firmware_html,HTTPD_RESP_USE_STRLEN);
}

static esp_err_t send_json(httpd_req_t *req, cJSON *root)
{
    if (!root) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t result = httpd_resp_sendstr(req, body);
    free(body);
    return result;
}

static esp_err_t handle_settings_page(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, lf_basic_settings_html, HTTPD_RESP_USE_STRLEN);
}

static bool number_in_range(const cJSON *root, const char *name,
                            int minimum, int maximum, int *out)
{
    const cJSON *node = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!cJSON_IsNumber(node) || node->valuedouble != (double)node->valueint ||
        node->valueint < minimum || node->valueint > maximum) return false;
    *out = node->valueint;
    return true;
}

static bool boolean_value(const cJSON *root, const char *name, uint8_t *out)
{
    const cJSON *node = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!cJSON_IsBool(node)) return false;
    *out = cJSON_IsTrue(node) ? 1 : 0;
    return true;
}

/* Public read-only state; no PIN, Wi-Fi password or cloud credentials. */

/* Global device and lyrics controls (no PIN). Settings are applied under
   the display lock and committed to the existing NVS namespace. */
static esp_err_t handle_preferences_get(httpd_req_t *req)
{
    lf_settings_t p;
    if(lf_settings_load(&p)!=ESP_OK)
        return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Cannot read preferences");
    cJSON *root=cJSON_CreateObject();
    if(!root)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
    cJSON_AddNumberToObject(root,"brightness",p.brightness);
    cJSON_AddNumberToObject(root,"scroll_speed",p.scroll_speed);
    cJSON_AddNumberToObject(root,"sleep_seconds",p.sleep_seconds);
    cJSON_AddNumberToObject(root,"deep_sleep_minutes",p.deep_sleep_minutes);
    cJSON_AddNumberToObject(root,"font_size",p.font_size);
    cJSON_AddNumberToObject(root,"system_font_size",p.system_font_size);
    cJSON_AddNumberToObject(root,"charging_color",p.charging_color);
    return send_json(req,root);
}
static esp_err_t handle_preferences_post(httpd_req_t *req)
{
    if(req->content_len<2||req->content_len>440)
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid preferences request");
    char json[441]={0};int got=0,timeouts=0;
    while(got<req->content_len){
        int n=httpd_req_recv(req,json+got,req->content_len-got);
        if(n==HTTPD_SOCK_ERR_TIMEOUT&&++timeouts<4)continue;
        if(n<=0)return httpd_resp_send_err(req,HTTPD_408_REQ_TIMEOUT,"Preferences upload interrupted");
        timeouts=0;got+=n;
    }
    cJSON *root=cJSON_Parse(json);
    lf_settings_t p;
    int brightness,scroll,timeout,deepmins,font,system,colour;
    bool valid=root&&cJSON_IsObject(root)&&lf_settings_load(&p)==ESP_OK&&
        number_in_range(root,"brightness",5,100,&brightness)&&
        number_in_range(root,"scroll_speed",0,100,&scroll)&&
        number_in_range(root,"sleep_seconds",0,60,&timeout)&&
        (timeout==0||timeout==15||timeout==30||timeout==60)&&
        number_in_range(root,"deep_sleep_minutes",0,60,&deepmins)&&
        (deepmins==0||deepmins==5||deepmins==15||deepmins==30||deepmins==60)&&
        number_in_range(root,"font_size",0,2,&font)&&
        number_in_range(root,"system_font_size",0,2,&system)&&
        number_in_range(root,"charging_color",0,0xFFFFFF,&colour);
    cJSON_Delete(root);
    if(!valid)return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid device preferences");
    p.brightness=(uint8_t)brightness;
    p.scroll_speed=(uint8_t)scroll;
    p.sleep_seconds=(uint8_t)timeout;
    p.deep_sleep_minutes=(uint16_t)deepmins;
    p.font_size=(uint8_t)font;
    p.system_font_size=(uint8_t)system;
    p.charging_color=(uint32_t)colour;
    if(lf_ui_apply_device_settings(&p)!=ESP_OK)
        return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Could not apply preferences");
    cJSON *answer=cJSON_CreateObject();
    if(!answer)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
    cJSON_AddBoolToObject(answer,"saved",true);
    return send_json(req,answer);
}
static esp_err_t handle_basic_get(httpd_req_t *req)
{
    lf_settings_t s;
    if (lf_settings_load(&s) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not read settings");
    lf_weather_config_t weather;
    lf_weather_get_config(&weather);
    cJSON *root = cJSON_CreateObject();
    if (!root) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
    cJSON_AddNumberToObject(root, "clock_face", s.clock_face);
    cJSON *faces=cJSON_AddArrayToObject(root,"face_profiles");
    if(faces) for(unsigned i=0;i<6;i++){
        cJSON *p=cJSON_CreateObject();
        if(!p) break;
        cJSON_AddNumberToObject(p,"background",s.face_background[i]);
        cJSON_AddNumberToObject(p,"foreground",s.face_foreground[i]);
        cJSON_AddNumberToObject(p,"accent",s.face_accent[i]);
        cJSON_AddBoolToObject(p,"show_date",s.face_show_date[i]!=0);
        cJSON_AddBoolToObject(p,"show_seconds",s.face_show_seconds[i]!=0);
        cJSON_AddBoolToObject(p,"show_battery",s.face_show_battery[i]!=0);
        cJSON_AddBoolToObject(p,"time_24h",s.face_24h[i]!=0);
        cJSON_AddItemToArray(faces,p);
    }
    cJSON_AddBoolToObject(root, "weather_enabled", weather.enabled);
    cJSON_AddNumberToObject(root, "font_style", s.basic_font_style);
    cJSON_AddNumberToObject(root, "font_size", s.basic_font_size);
    cJSON_AddNumberToObject(root, "ring_mode", s.basic_ring_mode);
    cJSON_AddBoolToObject(root, "show_date", s.basic_show_date != 0);
    cJSON_AddBoolToObject(root, "weather", s.basic_weather != 0);
    cJSON_AddNumberToObject(root, "background", s.basic_background);
    cJSON_AddNumberToObject(root, "foreground", s.basic_foreground);
    cJSON_AddNumberToObject(root, "ring_a", s.basic_ring_a);
    cJSON_AddNumberToObject(root, "ring_b", s.basic_ring_b);
    cJSON_AddNumberToObject(root, "ring_c", s.basic_ring_c);
    cJSON_AddStringToObject(root, "city", weather.city);
    cJSON_AddNumberToObject(root, "interval_minutes", weather.interval_minutes);
    return send_json(req, root);
}

static esp_err_t handle_basic_post(httpd_req_t *req)
{
    if (req->content_len < 2 || req->content_len > 2500) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request length");
        return ESP_FAIL;
    }
    char json[2501];
    int received = 0, retries = 0;
    while (received < req->content_len) {
        int n = httpd_req_recv(req, json + received, req->content_len - received);
        if (n == HTTPD_SOCK_ERR_TIMEOUT && ++retries < 4) continue;
        if (n <= 0) {
            lf_diag_error("settings", "Web request receive timeout/disconnect");
            httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Settings request timed out");
            return ESP_FAIL;
        }
        retries = 0;
        received += n;
    }
    json[received] = 0;
    cJSON *root = cJSON_Parse(json);
    lf_settings_t current;
    lf_weather_config_t weather;
    lf_weather_get_config(&weather);
    if (!root || !cJSON_IsObject(root) || lf_settings_load(&current) != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid settings JSON");
        return ESP_FAIL;
    }
    int face, font_style, font_size, ring_mode, bg, fg, a, b, c, interval;
    uint8_t show_date, basic_weather, weather_enabled;
    const cJSON *city = cJSON_GetObjectItemCaseSensitive(root, "city");
    bool valid =
        number_in_range(root, "clock_face", 0, 5, &face) &&
        boolean_value(root, "weather_enabled", &weather_enabled) &&
        number_in_range(root, "font_style", 0, 4, &font_style) &&
        number_in_range(root, "font_size", 0, 100, &font_size) &&
        number_in_range(root, "ring_mode", 0, 2, &ring_mode) &&
        boolean_value(root, "show_date", &show_date) &&
        boolean_value(root, "weather", &basic_weather) &&
        number_in_range(root, "background", 0, 0xFFFFFF, &bg) &&
        number_in_range(root, "foreground", 0, 0xFFFFFF, &fg) &&
        number_in_range(root, "ring_a", 0, 0xFFFFFF, &a) &&
        number_in_range(root, "ring_b", 0, 0xFFFFFF, &b) &&
        number_in_range(root, "ring_c", 0, 0xFFFFFF, &c) &&
        number_in_range(root, "interval_minutes", 15, 120, &interval) &&
        (interval == 15 || interval == 30 || interval == 60 || interval == 120) &&
        cJSON_IsString(city) && city->valuestring &&
        strlen(city->valuestring) <= LF_WEATHER_CITY_MAX;
    const cJSON *profiles=cJSON_GetObjectItemCaseSensitive(root,"face_profiles");
    uint32_t bg_face[6],fg_face[6],ac_face[6];
    uint8_t dt_face[6],sc_face[6],bt_face[6],hr_face[6];
    if(!cJSON_IsArray(profiles)||cJSON_GetArraySize(profiles)!=6) valid=false;
    if(valid) for(int i=0;i<6;i++){
        const cJSON *p=cJSON_GetArrayItem(profiles,i);
        int bgv,fgv,acv;uint8_t dv,sv,bv,hv;
        if(!p || !number_in_range(p,"background",0,0xFFFFFF,&bgv)
           || !number_in_range(p,"foreground",0,0xFFFFFF,&fgv)
           || !number_in_range(p,"accent",0,0xFFFFFF,&acv)
           || !boolean_value(p,"show_date",&dv)
           || !boolean_value(p,"show_seconds",&sv)
           || !boolean_value(p,"show_battery",&bv)
           || !boolean_value(p,"time_24h",&hv)){valid=false;break;}
        bg_face[i]=(uint32_t)bgv;fg_face[i]=(uint32_t)fgv;ac_face[i]=(uint32_t)acv;
        dt_face[i]=dv;sc_face[i]=sv;bt_face[i]=bv;hr_face[i]=hv;
    }
    if (valid) {
        const char *p = city->valuestring;
        for (; *p; p++) if ((unsigned char)*p < 32) { valid = false; break; }
    }
    if (!valid) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid clock settings");
        return ESP_FAIL;
    }
    current.clock_face = (uint8_t)face;
    current.basic_font_style = (uint8_t)font_style;
    current.basic_font_size = (uint8_t)font_size;
    current.basic_ring_mode = (uint8_t)ring_mode;
    current.basic_show_date = show_date;
    current.basic_weather = basic_weather;
    current.basic_background = (uint32_t)bg;
    current.basic_foreground = (uint32_t)fg;
    current.basic_ring_a = (uint32_t)a;
    current.basic_ring_b = (uint32_t)b;
    current.basic_ring_c = (uint32_t)c;
    for(int i=0;i<6;i++){
        current.face_background[i]=bg_face[i];current.face_foreground[i]=fg_face[i];
        current.face_accent[i]=ac_face[i];current.face_show_date[i]=dt_face[i];
        current.face_show_seconds[i]=sc_face[i];current.face_show_battery[i]=bt_face[i];
        current.face_24h[i]=hr_face[i];
    }
    /* Basic face keeps existing ring/font controls as well. */
    current.basic_background=current.face_background[5];
    current.basic_foreground=current.face_foreground[5];
    current.basic_show_date=current.face_show_date[5];
    strlcpy(weather.city, city->valuestring, sizeof weather.city);
    weather.interval_minutes = (uint16_t)interval;
    weather.enabled = weather_enabled != 0;
    cJSON_Delete(root);

    if (!lf_weather_save_config(&weather)) {
        lf_diag_error("settings", "Weather NVS save failed");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot save weather settings");
        return ESP_FAIL;
    }
    esp_err_t err = lf_ui_apply_basic_settings(&current);
    if (err != ESP_OK) {
        lf_diag_error("settings", "UI lock or NVS save failed");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot save clock settings");
        return ESP_FAIL;
    }
    cJSON *reply = cJSON_CreateObject();
    if (!reply) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
    cJSON_AddBoolToObject(reply, "saved", true);
    return send_json(req, reply);
}

static esp_err_t handle_diagnostics(httpd_req_t *req)
{
    lf_diag_snapshot_t d;
    lf_diag_snapshot(&d);
    cJSON *root = cJSON_CreateObject();
    if (!root) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
    cJSON_AddStringToObject(root, "version", LF_VERSION);
    cJSON_AddStringToObject(root, "ip", d.ip_address);
    cJSON_AddBoolToObject(root, "wifi_connected", d.wifi_connected);
    cJSON_AddStringToObject(root, "wifi_ssid", d.wifi_ssid);
    if (d.wifi_rssi_dbm != 0) cJSON_AddNumberToObject(root, "wifi_rssi_dbm", d.wifi_rssi_dbm);
    cJSON_AddNumberToObject(root, "uptime_seconds", d.uptime_seconds);
    cJSON_AddNumberToObject(root, "free_heap_bytes", d.heap_free_bytes);
    cJSON_AddNumberToObject(root, "min_free_heap_bytes", d.heap_min_bytes);
    cJSON_AddNumberToObject(root, "largest_free_block_bytes", d.heap_largest_block_bytes);
    cJSON_AddNumberToObject(root, "reset_reason_code", d.reset_reason);
    cJSON_AddNumberToObject(root, "freertos_task_count", d.task_count);
    cJSON_AddNumberToObject(root, "free_psram_bytes", d.psram_free_bytes);
    cJSON_AddBoolToObject(root, "battery_valid", d.battery_valid);
    if (d.battery_valid) {
        cJSON_AddNumberToObject(root, "battery_percent", d.battery.percentage);
        cJSON_AddBoolToObject(root, "usb_connected", d.battery.usb_connected);
        cJSON_AddBoolToObject(root, "charging", d.battery.charging);
    }
    cJSON_AddStringToObject(root, "microphone", "disabled (not initialized)");
    cJSON_AddStringToObject(root, "ota_result", ota_result);
    cJSON_AddStringToObject(root, "ota_detail", ota_detail);
    cJSON_AddNumberToObject(root, "ota_error_code", atomic_load(&ota_error_code));
    cJSON_AddNumberToObject(root, "ota_http_status", atomic_load(&ota_http_status));
    cJSON_AddNumberToObject(root, "ota_tls_code", atomic_load(&ota_tls_code));
    cJSON_AddNumberToObject(root, "ota_tls_flags", atomic_load(&ota_tls_flags));
    cJSON_AddNumberToObject(root, "ota_socket_errno", atomic_load(&ota_socket_errno));
    cJSON_AddNumberToObject(root, "ota_received", ota_received);
    cJSON_AddNumberToObject(root, "ota_total", ota_total);
    cJSON_AddNumberToObject(root, "max_ui_tick_gap_ms", lf_ui_max_tick_gap_ms());
    lf_ui_display_stats_t display_stats={0};
    lf_ui_display_stats(&display_stats);
    cJSON_AddNumberToObject(root, "ui_touch_presses", display_stats.touch_presses);
    cJSON_AddNumberToObject(root, "ui_navigation_commits", display_stats.navigation_commits);
    cJSON_AddNumberToObject(root, "ui_full_redraws", display_stats.full_redraws);
    cJSON_AddNumberToObject(root, "display_render_count", display_stats.render_count);
    cJSON_AddNumberToObject(root, "display_flush_starts", display_stats.flush_starts);
    cJSON_AddNumberToObject(root, "display_flush_finishes", display_stats.flush_finishes);
    cJSON_AddNumberToObject(root, "display_refresh_ready", display_stats.refresh_ready);
    cJSON_AddNumberToObject(root, "clock_scene_builds", display_stats.clock_scene_builds);
    cJSON_AddNumberToObject(root, "clock_cache_hits", display_stats.clock_cache_hits);
    cJSON_AddNumberToObject(root, "clock_build_last_ms", display_stats.clock_build_last_ms);
    cJSON_AddNumberToObject(root, "clock_build_max_ms", display_stats.clock_build_max_ms);
    cJSON_AddNumberToObject(root, "clock_present_last_ms", display_stats.clock_present_last_ms);
    cJSON_AddNumberToObject(root, "clock_present_max_ms", display_stats.clock_present_max_ms);
    lf_weather_config_t weather;
    lf_weather_get_config(&weather);
    cJSON_AddStringToObject(root, "weather_city", weather.city);
    cJSON *errors = cJSON_AddArrayToObject(root, "recent_errors");
    if (errors)
        for (size_t i = 0; i < 3; i++)
            if (d.errors[i][0]) cJSON_AddItemToArray(errors, cJSON_CreateString(d.errors[i]));
    return send_json(req, root);
}

static esp_err_t handle_info(httpd_req_t *req)
{
    char ssid[33] = {0};
    char saved[33] = {0};
    (void)lf_network_connected_ssid(ssid, sizeof ssid);
    nvs_handle_t h;
    if (nvs_open("lf_network", NVS_READONLY, &h) == ESP_OK) {
        size_t max = sizeof saved;
        (void)nvs_get_str(h, "ssid", saved, &max);
        nvs_close(h);
    }
    cJSON *root = cJSON_CreateObject();
    if (!root) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
    cJSON_AddStringToObject(root, "app", "LyricFlash");
    cJSON_AddStringToObject(root, "version", LF_VERSION);
    cJSON_AddStringToObject(root, "mode", in_ap ? "ap" : "station");
    cJSON_AddStringToObject(root, "address", address);
    cJSON_AddStringToObject(root, "ota_result", ota_result);
    cJSON_AddStringToObject(root, "ota_detail", ota_detail);
    cJSON_AddNumberToObject(root, "ota_error_code", atomic_load(&ota_error_code));
    cJSON_AddNumberToObject(root, "ota_http_status", atomic_load(&ota_http_status));
    cJSON_AddNumberToObject(root, "ota_tls_code", atomic_load(&ota_tls_code));
    cJSON_AddNumberToObject(root, "ota_tls_flags", atomic_load(&ota_tls_flags));
    cJSON_AddNumberToObject(root, "ota_socket_errno", atomic_load(&ota_socket_errno));
    cJSON_AddNumberToObject(root, "ota_received", ota_received);
    cJSON_AddNumberToObject(root, "ota_total", ota_total);
    cJSON_AddStringToObject(root, "connected_ssid", ssid);
    cJSON_AddStringToObject(root, "saved_ssid", saved);
    cJSON_AddBoolToObject(root, "wifi_saved", saved[0] != 0);
    cJSON_AddStringToObject(root, "setup_ssid", in_ap ? ap_ssid : "");
    /* Never expose setup-network passwords via an unauthenticated HTTP API. */
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
    httpd_resp_set_type(req, "application/json");
    esp_err_t r = httpd_resp_sendstr(req, body);
    free(body);
    return r;
}
static esp_err_t handle_device(httpd_req_t *req)
{
    uint8_t mac[6]={0};
    esp_wifi_get_mac(in_ap?WIFI_IF_AP:WIFI_IF_STA,mac);
    char response[220];
    snprintf(response,sizeof(response),
        "{\"app\":\"LyricFlash\",\"name\":\"LyricFlash 1.75\",\"version\":\"0.8.4-alpha\","
        "\"device_id\":\"LF-%02X%02X%02X\",\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\"}",
        mac[3],mac[4],mac[5],mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    httpd_resp_set_type(req,"application/json");
    return httpd_resp_sendstr(req,response);
}
static esp_err_t handle_wifi(httpd_req_t *req)
{
    /* LAN Settings page must also be able to change the saved home SSID.
       This endpoint is available on the local device network without PIN,
       as explicitly requested; no credential is returned by API. */
    if(req->content_len<2 || req->content_len>320){httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid request size");return ESP_FAIL;}
    char json[321]; int n=0;
    while(n<req->content_len){int r=httpd_req_recv(req,json+n,req->content_len-n);if(r<=0){httpd_resp_send_err(req,HTTPD_408_REQ_TIMEOUT,"Request timed out");return ESP_FAIL;}n+=r;}
    json[n]=0;
    /* Tiny and dependency-free JSON parser deliberately avoided: cJSON handles escaping. */
    extern bool lf_wifi_parse_and_save(const char *body);
    if(!lf_wifi_parse_and_save(json)){httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Wi-Fi name or password invalid");return ESP_FAIL;}
    httpd_resp_sendstr(req,"Saved. ESP32 restarting; connect to your home Wi-Fi.");
    lf_network_restart();
    return ESP_OK;
}
/* GET /api/wifi-scan: starts a scan or returns the latest results.
   Result is transient, never includes saved Wi-Fi passwords. */
static esp_err_t handle_scan(httpd_req_t *req)
{
    lf_wifi_ap_t networks[LF_WIFI_MAX_RESULTS] = {0};
    size_t count = 0;
    int state = lf_network_scan_results(networks, LF_WIFI_MAX_RESULTS, &count);
    char query[28] = {0};
    (void)httpd_req_get_url_query_str(req, query, sizeof query);
    if (state != 1 && strcmp(query, "refresh=1") == 0) {
        if (lf_network_scan_start()) {
            state = 1;
            count = 0;
        }
    }
    cJSON *root = cJSON_CreateObject();
    cJSON *items = cJSON_AddArrayToObject(root, "networks");
    cJSON_AddStringToObject(root, "state", state == 1 ? "scanning" :
                                        state == 2 ? "ready" : "error");
    if (state == 2 && items) {
        for (size_t i = 0; i < count; ++i) {
            cJSON *item = cJSON_CreateObject();
            if (!item) break;
            cJSON_AddStringToObject(item, "ssid", networks[i].ssid);
            cJSON_AddBoolToObject(item, "secured", networks[i].secured);
            cJSON_AddNumberToObject(item, "rssi", networks[i].rssi);
            cJSON_AddItemToArray(items, item);
        }
    }
    char *json = root ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    if (!json) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan response failed");
    httpd_resp_set_type(req, "application/json");
    esp_err_t status = httpd_resp_sendstr(req, json);
    free(json);
    return status;
}
static esp_err_t handle_update(httpd_req_t *req)
{
    if (atomic_exchange(&ota_busy,true)) {
        httpd_resp_set_status(req,"409 Conflict");
        httpd_resp_sendstr(req,"Another firmware update is already in progress");
        return ESP_FAIL;
    }
    ota_received=0;
    ota_total=req->content_len>0?(uint32_t)req->content_len:0;
    ota_result="validating";
    const esp_partition_t *part=esp_ota_get_next_update_partition(NULL);
    if(!part || req->content_len<512 || req->content_len>(int)part->size){
        ota_result="bad_size"; ESP_LOGE(TAG,"Invalid OTA request size / inactive partition");
        httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"OTA image invalid or too large");atomic_store(&ota_busy,false);return ESP_FAIL;
    }
    uint8_t buf[2048];
    uint8_t first[0x120];
    size_t first_len=0;
    int received=0;
    esp_ota_handle_t handle=0;
    bool started=false;
    bool ok=true;
    int receive_timeouts=0;
    while(received<req->content_len) {
        int remaining=req->content_len-received;
        int r=httpd_req_recv(req,(char *)buf,remaining>(int)sizeof(buf)?sizeof(buf):remaining);
        if(r==HTTPD_SOCK_ERR_TIMEOUT && ++receive_timeouts<4) continue;
        if(r<=0){ESP_LOGE(TAG,"OTA receive failed code=%d at %d/%d",r,received,req->content_len);ota_result="receive_failed";ok=false;break;}
        receive_timeouts=0;
        if(!started){
            size_t copy=(size_t)r;
            if(copy>sizeof(first)-first_len) copy=sizeof(first)-first_len;
            memcpy(first+first_len,buf,copy);
            first_len+=copy;
            if(first_len>=sizeof(first)){
                lf_image_result_t check=lf_check_ota_image(first,sizeof(first),part->size);
                if(check!=LF_IMAGE_OK){ESP_LOGE(TAG,"OTA header: %s",lf_image_error(check));ota_result="header_failed";ok=false;break;}
                /* Erase only the image size rather than pre-erasing the entire 5 MB slot. */
                esp_err_t begin_err=esp_ota_begin(part,(size_t)req->content_len,&handle);
                if(begin_err!=ESP_OK){ESP_LOGE(TAG,"OTA begin: %s",esp_err_to_name(begin_err));ota_result="begin_failed";ok=false;break;}
                started=true;
                if(esp_ota_write(handle,first,first_len)!=ESP_OK){ok=false;break;}
                if((size_t)r>copy && esp_ota_write(handle,buf+copy,r-copy)!=ESP_OK){ok=false;break;}
            }
        } else if(esp_ota_write(handle,buf,r)!=ESP_OK){ok=false;break;}
        received+=r;
        ota_received=(uint32_t)received;
        ota_result="writing";
    }
    if(!ok){lf_diag_error("ota",ota_result);if(started) esp_ota_abort(handle);ESP_LOGE(TAG,"OTA incomplete at %u/%u",ota_received,ota_total);
        httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Upload failed, device remains on current firmware");atomic_store(&ota_busy,false);return ESP_FAIL;}
    ota_result="verifying";
    esp_err_t finish_err=esp_ota_end(handle);
    if(finish_err!=ESP_OK){ota_result="checksum_failed";lf_diag_error("ota",ota_result);ESP_LOGE(TAG,"OTA end: %s",esp_err_to_name(finish_err));httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Firmware checksum / image validation failed");atomic_store(&ota_busy,false);return ESP_FAIL;}
    esp_err_t boot_err=esp_ota_set_boot_partition(part);
    if(boot_err!=ESP_OK){ota_result="activation_failed";lf_diag_error("ota",ota_result);ESP_LOGE(TAG,"OTA activate: %s",esp_err_to_name(boot_err));httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Cannot activate new firmware");atomic_store(&ota_busy,false);return ESP_FAIL;}
    ota_result="verified_rebooting";
    httpd_resp_sendstr(req,"Firmware verified. Rebooting into new application.");
    xTaskCreate(reboot_task,"lf_reboot",2048,NULL,3,NULL);
    return ESP_OK;
}

/* Every direct GitHub OTA runs in its own worker, never on LVGL or HTTP server.
   PIN-free does NOT mean bypassing chip checks, TLS validation or NVS isolation. */
static bool allowed_github_ota_url(const char *url)
{
    const char *beta = "https://skiiirt1983.github.io/LyricFlash/beta/firmware/lyricflash-ota.bin";
    const char *releases = "https://github.com/skiiirt1983/LyricFlash/releases/download/";
    /* Signed and versioned release binaries on a separate branch avoid the
       multi-host GitHub Releases download redirect chain. */
    const char *direct = "https://raw.githubusercontent.com/skiiirt1983/LyricFlash/alpha-ota/releases/";
    if (url && strncmp(url,direct,strlen(direct))==0) {
        const char *path=url+strlen(direct);
        const char *file="/lyricflash-ota.bin";
        if(strncmp(path,"alpha-build-",12)!=0 || strlen(path)!=12+12+strlen(file) ||
           strcmp(path+24,file)!=0)return false;
        for(int i=12;i<24;i++) {
            char c=path[i]; if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
        }
        return true;
    }
    if (!url || strlen(url) >= sizeof(((lf_github_request_t *)0)->url)) return false;
    if (strcmp(url, beta) == 0) return true;
    const char *history="https://skiiirt1983.github.io/LyricFlash/beta/firmware/releases/";
    if(strncmp(url,history,strlen(history))==0){
        const char *path=url+strlen(history);
        const char *separator=strchr(path,'/');
        if(!separator || separator==path ||
           strcmp(separator,"/lyricflash-ota.bin")!=0 ||
           (size_t)(separator-path)>64) return false;
        for(const char *c=path;c<separator;c++)
            if(!((*c>='0'&&*c<='9')||(*c>='A'&&*c<='Z')||
                 (*c>='a'&&*c<='z')||*c=='.'||*c=='-'||*c=='_')) return false;
        return path[0]!='.';
    }
    if (strncmp(url, releases, strlen(releases)) != 0) return false;
    const char *path=url+strlen(releases);
    if (!*path || strstr(path,"..") || strchr(path,'?') || strchr(path,'#') ||
        strchr(path,'%') || strchr(path,'@') || strchr(path,'\\')) return false;
    const char *sep=strchr(path,'/');
    if (!sep || sep==path || !sep[1] || strchr(sep+1,'/')) return false;
    const char *filename=sep+1;
    const size_t length=strlen(filename);
    if (length < 8 || strcmp(filename+length-4,".bin") != 0) return false;
    return strstr(filename,"ota") || strstr(filename,"OTA");
}

/* HTTPS OTA can redirect GitHub release URLs to release-assets hosts.
   Record the final HTTP status; never disable TLS peer verification. */
static esp_err_t ota_http_event(esp_http_client_event_t *event)
{
    if (event && event->client) {
        if(event->event_id==HTTP_EVENT_ON_HEADER ||
           event->event_id==HTTP_EVENT_ON_FINISH) {
            int status=esp_http_client_get_status_code(event->client);
            if(status>0)atomic_store(&ota_http_status,status);
        }
        if(event->event_id==HTTP_EVENT_ERROR) {
            int tls_code=0,flags=0;
            esp_err_t tls=esp_http_client_get_and_clear_last_tls_error(event->client,&tls_code,&flags);
            atomic_store(&ota_tls_code,tls_code ? tls_code : (int)tls);
            atomic_store(&ota_tls_flags,flags);
            atomic_store(&ota_socket_errno,esp_http_client_get_errno(event->client));
        }
    }
    return ESP_OK;
}

static void github_ota_worker(void *arg)
{
    lf_github_request_t *job=(lf_github_request_t *)arg;
    ota_result="connecting";
    ota_detail="";
    atomic_store(&ota_error_code,0);
    atomic_store(&ota_http_status,0);
    atomic_store(&ota_tls_code,0);
    atomic_store(&ota_tls_flags,0);
    atomic_store(&ota_socket_errno,0);
    /* Setup AP is local-only. Do not present its lack of Internet as a TLS error. */
    if (!lf_network_is_connected()) {
        ota_result="no_internet";
        ota_detail="Connect LyricFlash to a Wi-Fi network with Internet access.";
        goto done;
    }
    /* HTTPS certificate dates require real UTC, not the local PCT display time. */
    time_t utc=time(NULL);
    struct tm calendar={0};
    gmtime_r(&utc,&calendar);
    if(calendar.tm_year+1900<2025) {
        if(!time_sync_started)start_time_sync();
        ota_result="syncing_time";
        (void)esp_netif_sntp_sync_wait(pdMS_TO_TICKS(18000));
        utc=time(NULL);
        gmtime_r(&utc,&calendar);
        if(calendar.tm_year+1900<2025){
            ota_result="time_unsynced";
            ota_detail="Device clock has not synchronized via NTP; HTTPS certificates cannot be validated.";
            goto done;
        }
    }
    ota_result="connecting";
    esp_http_client_config_t config = {
        .url=job->url,
        .crt_bundle_attach=esp_crt_bundle_attach,
        .timeout_ms=30000,
        .keep_alive_enable=false,
        .max_redirection_count=10,
        .disable_auto_redirect=false,
        .event_handler=ota_http_event,
        .user_agent="LyricFlash-OTA/0.8",
        .buffer_size=4096
    };
    esp_https_ota_config_t ota_config={.http_config=&config};
    esp_https_ota_handle_t handle=NULL;
    esp_err_t err=esp_https_ota_begin(&ota_config,&handle);
    if(err!=ESP_OK) {
        ESP_LOGE(TAG,"GitHub OTA connection failed: %s",esp_err_to_name(err));
        ota_result="download_failed";
        ota_detail="HTTPS connection failed. Check network access, GitHub redirects and TLS trust.";
        atomic_store(&ota_error_code,(int)err);
        goto done;
    }
    esp_app_desc_t descriptor={0};
    err=esp_https_ota_get_img_desc(handle,&descriptor);
    if(err!=ESP_OK || strcmp(descriptor.project_name,"lyricflash")!=0 ||
       strcmp(descriptor.version,job->version)!=0) {
        ota_result="bad_version";
        ota_detail="Downloaded image is not the selected LyricFlash version.";
        atomic_store(&ota_error_code,(int)err);
        ESP_LOGE(TAG,"GitHub OTA app descriptor differs from selected LyricFlash version");
        esp_https_ota_abort(handle);
        goto done;
    }
    ota_total=(uint32_t)esp_https_ota_get_image_size(handle);
    const esp_partition_t *slot=esp_ota_get_next_update_partition(NULL);
    if(!slot || (ota_total && ota_total>slot->size)) {
        ota_result="bad_size";
        ota_detail="OTA image is larger than the available firmware partition.";
        esp_https_ota_abort(handle);
        goto done;
    }
    ota_result="downloading";
    while((err=esp_https_ota_perform(handle))==ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
        int n=esp_https_ota_get_image_len_read(handle);
        if(n>=0) ota_received=(uint32_t)n;
    }
    if(err!=ESP_OK || !esp_https_ota_is_complete_data_received(handle)) {
        ota_result="download_failed";
        ota_detail="HTTPS download interrupted or incomplete.";
        atomic_store(&ota_error_code,(int)err);
        ESP_LOGE(TAG,"GitHub OTA transfer failed: %s",esp_err_to_name(err));
        esp_https_ota_abort(handle);
        goto done;
    }
    ota_result="verifying";
    err=esp_https_ota_finish(handle);
    if(err!=ESP_OK) {
        ota_result="checksum_failed";
        ota_detail="Downloaded OTA failed ESP-IDF image verification.";
        atomic_store(&ota_error_code,(int)err);
        ESP_LOGE(TAG,"GitHub OTA validation failed: %s",esp_err_to_name(err));
        goto done;
    }
    ota_result="verified_rebooting";
    ota_detail="Firmware verified; rebooting.";
    xTaskCreate(reboot_task,"lf_reboot",2048,NULL,3,NULL);
done:
    free(job);
    if(strcmp(ota_result,"verified_rebooting")!=0) atomic_store(&ota_busy,false);
    vTaskDelete(NULL);
}

static esp_err_t handle_github_install(httpd_req_t *req)
{
    if(req->content_len<2 || req->content_len>600)
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid install request");
    char json[601]={0};
    int got=0;
    while(got<req->content_len) {
        int n=httpd_req_recv(req,json+got,req->content_len-got);
        if(n<=0) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Incomplete install request");
        got+=n;
    }
    cJSON *root=cJSON_Parse(json);
    const cJSON *url=root?cJSON_GetObjectItemCaseSensitive(root,"url"):NULL;
    const cJSON *ver=root?cJSON_GetObjectItemCaseSensitive(root,"version"):NULL;
    lf_github_request_t *job=calloc(1,sizeof(*job));
    bool valid=job && cJSON_IsString(url) && cJSON_IsString(ver) &&
        url->valuestring && ver->valuestring &&
        strlen(ver->valuestring)>0 && strlen(ver->valuestring)<sizeof(job->version) &&
        allowed_github_ota_url(url->valuestring);
    if(valid) {
        strlcpy(job->url,url->valuestring,sizeof(job->url));
        strlcpy(job->version,ver->valuestring,sizeof(job->version));
    }
    cJSON_Delete(root);
    if(!valid){free(job);return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Unsupported LyricFlash GitHub OTA URL or version");}
    if(atomic_exchange(&ota_busy,true)){
        free(job);
        httpd_resp_set_status(req,"409 Conflict");
        return httpd_resp_sendstr(req,"Another firmware update is active");
    }
    ota_received=0;ota_total=0;ota_result="queued";ota_detail="";
    atomic_store(&ota_error_code,0);atomic_store(&ota_http_status,0);
    atomic_store(&ota_tls_code,0);atomic_store(&ota_tls_flags,0);
    atomic_store(&ota_socket_errno,0);
    if(xTaskCreate(github_ota_worker,"lf_github_ota",12288,job,tskIDLE_PRIORITY+2,NULL)!=pdPASS){
        atomic_store(&ota_busy,false);free(job);ota_result="download_failed";
        return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Cannot start OTA download");
    }
    httpd_resp_set_status(req,"202 Accepted");
    return httpd_resp_sendstr(req,"Firmware download started on device");
}

static bool serve(void)
{
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();
    cfg.server_port=80; cfg.stack_size=8192;
    cfg.max_uri_handlers=18; /* Web settings/telemetry add routes beyond IDF default. */
    cfg.recv_wait_timeout=15; cfg.send_wait_timeout=15;
    if(httpd_start(&server,&cfg)!=ESP_OK) return false;
    const httpd_uri_t routes[]={
        {.uri="/",.method=HTTP_GET,.handler=handle_home},
        {.uri="/firmware",.method=HTTP_GET,.handler=handle_firmware_page},
        {.uri="/api/install",.method=HTTP_POST,.handler=handle_github_install},
        {.uri="/api/info",.method=HTTP_GET,.handler=handle_info},
        {.uri="/settings",.method=HTTP_GET,.handler=handle_settings_page},
        {.uri="/api/preferences",.method=HTTP_GET,.handler=handle_preferences_get},
        {.uri="/api/preferences",.method=HTTP_POST,.handler=handle_preferences_post},
        {.uri="/api/basic",.method=HTTP_GET,.handler=handle_basic_get},
        {.uri="/api/basic",.method=HTTP_POST,.handler=handle_basic_post},
        {.uri="/api/diagnostics",.method=HTTP_GET,.handler=handle_diagnostics},
        {.uri="/api/device",.method=HTTP_GET,.handler=handle_device},
        {.uri="/wifi",.method=HTTP_POST,.handler=handle_wifi},
        {.uri="/api/wifi-scan",.method=HTTP_GET,.handler=handle_scan},
        {.uri="/update",.method=HTTP_POST,.handler=handle_update}
    };
    for(size_t i=0;i<sizeof(routes)/sizeof(routes[0]);i++)
        if(httpd_register_uri_handler(server,&routes[i])!=ESP_OK) return false;
    return true;
}
const char *lf_network_address(void){return address;}
const char *lf_network_ota_result(void){return ota_result;}
uint32_t lf_network_ota_received(void){return ota_received;}
uint32_t lf_network_ota_total(void){return ota_total;}
const char *lf_network_ap_ssid(void){return ap_ssid;}
const char *lf_network_ap_password(void){return ap_pass;}
bool lf_network_start(void)
{
    if(!connected_event) connected_event=xEventGroupCreate();
    if(!connected_event) return false;
    if(esp_netif_init()!=ESP_OK) return false;
    esp_err_t event_init=esp_event_loop_create_default();
    if(event_init!=ESP_OK && event_init!=ESP_ERR_INVALID_STATE) return false;
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    if(esp_wifi_init(&init)!=ESP_OK) return false;
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&wifi_event,NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,&wifi_event,NULL));
    char ssid[33]={0},pass[65]={0};
    bool configured=nvs_wifi_load(ssid,sizeof(ssid),pass,sizeof(pass));
    if(configured){
        in_ap=false;
        wifi_config_t config={0};
        strlcpy((char *)config.sta.ssid,ssid,sizeof(config.sta.ssid));
        strlcpy((char *)config.sta.password,pass,sizeof(config.sta.password));
        config.sta.threshold.authmode=WIFI_AUTH_OPEN;
        if(esp_wifi_set_mode(WIFI_MODE_STA)==ESP_OK &&
           esp_wifi_set_config(WIFI_IF_STA,&config)==ESP_OK &&
           esp_wifi_start()==ESP_OK){
            esp_wifi_connect();
            EventBits_t bits=xEventGroupWaitBits(connected_event,LF_CONNECTED,pdFALSE,pdFALSE,pdMS_TO_TICKS(12000));
            if(bits & LF_CONNECTED){
                ESP_LOGI(TAG,"Connected to configured Wi-Fi at %s",address);
                start_time_sync();
                return serve();
            }
        }
        ESP_LOGW(TAG,"Saved home Wi-Fi did not connect; offering setup AP (credentials retained)");
        lf_diag_error("wifi", "Home network connection timed out; setup AP enabled");
        in_ap=true;
        esp_wifi_stop();
    }
    /* Fallback to a password-protected access point if home Wi-Fi is unavailable. */
    in_ap=true;
    uint8_t mac[6]={0};
    esp_wifi_get_mac(WIFI_IF_STA,mac);
    snprintf(ap_ssid,sizeof ap_ssid,"LyricFlash-%02X%02X",mac[4],mac[5]);
    /* Private device's intentionally simple WPA2 setup password.
       Never replace/erase the saved home Wi-Fi credentials. */
    strlcpy(ap_pass, "lyricflash", sizeof ap_pass);
    wifi_config_t apcfg={0};
    strlcpy((char *)apcfg.ap.ssid,ap_ssid,sizeof(apcfg.ap.ssid));
    strlcpy((char *)apcfg.ap.password,ap_pass,sizeof(apcfg.ap.password));
    apcfg.ap.ssid_len=strlen(ap_ssid);
    apcfg.ap.channel=6;
    apcfg.ap.max_connection=2;
    apcfg.ap.authmode=WIFI_AUTH_WPA2_PSK;
    if(esp_wifi_set_mode(WIFI_MODE_AP)!=ESP_OK ||
       esp_wifi_set_config(WIFI_IF_AP,&apcfg)!=ESP_OK ||
       esp_wifi_start()!=ESP_OK) return false;
    strlcpy(address,"192.168.4.1",sizeof(address));
    ESP_LOGI(TAG,"Provisioning AP active: %s",ap_ssid);
    return serve();
}
