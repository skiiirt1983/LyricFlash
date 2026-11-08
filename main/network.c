#include "network.h"
#include "ui.h"
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "settings.h"
#include "power.h"
#include "weather.h"
#include "diagnostics.h"
#include "mic_probe.h"
#include "mic_pcm.h"
#include "recognition.h"
#include "song_history.h"
#include "version.h"
#include "web_basic_html.h"
#include "basic_open_face.h"
#include "web_device_html.h"
#include "web_weather_html.h"
#include "captive_dns.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include <stdatomic.h>
#include "cJSON.h"
#include "image_policy.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include <time.h>
#include <stdlib.h>
#include "esp_event.h"
#include "esp_random.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_heap_caps.h"
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
/* Requests originate from LVGL; only this worker calls the Wi-Fi driver. */
static atomic_bool wifi_idle_requested = false;
static atomic_bool wifi_radio_suspended = false;
static atomic_bool wifi_worker_running = false;
static atomic_uint wifi_retry_after_ms;
static atomic_uint wifi_radio_sleep_cycles;
static void wifi_idle_worker(void *arg)
{
    (void)arg;
    for (;;) {
        const bool requested = atomic_load(&wifi_idle_requested);
        const bool suspended = atomic_load(&wifi_radio_suspended);
        if (requested == suspended) break;
        if (requested) {
            if (in_ap || atomic_load(&ota_busy)) break;
            /* Mark intentional shutdown before stop triggers DISCONNECTED. */
            atomic_store(&wifi_radio_suspended, true);
            esp_err_t err = esp_wifi_stop();
            if (err != ESP_OK) {
                atomic_store(&wifi_radio_suspended, false);
                atomic_store(&wifi_retry_after_ms,(uint32_t)(esp_timer_get_time()/1000LL)+1000U);
                ESP_LOGW(TAG, "Idle Wi-Fi stop failed: %s", esp_err_to_name(err));
                break;
            }
            if (connected_event) xEventGroupClearBits(connected_event, LF_CONNECTED);
            atomic_fetch_add(&wifi_radio_sleep_cycles,1);
            ESP_LOGI(TAG, "Idle Wi-Fi radio suspended");
        } else {
            esp_err_t err = esp_wifi_start();
            if (err != ESP_OK) {
                atomic_store(&wifi_retry_after_ms,(uint32_t)(esp_timer_get_time()/1000LL)+1000U);
                ESP_LOGW(TAG, "Wake Wi-Fi start failed: %s", esp_err_to_name(err));
                break;
            }
            atomic_store(&wifi_radio_suspended, false);
            esp_wifi_connect();
            ESP_LOGI(TAG, "Wake Wi-Fi reconnect requested");
        }
    }
    atomic_store(&wifi_worker_running, false);
    /* The UI tick retries a pending idle request; wake also retries on tick. */
    vTaskDelete(NULL);
}
void lf_network_set_idle(bool idle)
{
    if (idle && (in_ap || atomic_load(&ota_busy))) return;
    const bool previous=atomic_exchange(&wifi_idle_requested,idle);
    if(previous!=idle)atomic_store(&wifi_retry_after_ms,0);
    if (idle == atomic_load(&wifi_radio_suspended)) return;
    const uint32_t now=(uint32_t)(esp_timer_get_time()/1000LL);
    if((int32_t)(now-atomic_load(&wifi_retry_after_ms))<0)return;
    if (!atomic_exchange(&wifi_worker_running, true)) {
        if (xTaskCreate(wifi_idle_worker, "lf_wifi_idle", 3072, NULL, 4, NULL) != pdPASS)
            atomic_store(&wifi_worker_running, false);
    }
}


const char *lf_network_wifi_power_state(void)
{
    if(in_ap)return "setup_ap";
    if(atomic_load(&wifi_radio_suspended))return "screen_off_radio_off";
    return atomic_load(&wifi_worker_running)?"radio_transition":"radio_on";
}
uint32_t lf_network_radio_sleep_cycles(void)
{
    return atomic_load(&wifi_radio_sleep_cycles);
}
/* Safe from watchdog task: never restart the CPU while OTA is flashing. */
bool lf_network_ota_in_progress(void)
{
    return atomic_load(&ota_busy);
}

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

/* Pacific Time is the default. Later versions can expose time zone
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
        if (!atomic_load(&wifi_radio_suspended) && !atomic_load(&wifi_idle_requested)) esp_wifi_connect();
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

/* Lightweight cache-only endpoint: never run a slow hardware/diagnostics
 * probe just to paint the battery icon in the LyricFlash Web UI header. */
/* The Clock Studio requests the SAME low-resolution, open-licensed digit
 * glyphs used by firmware. Rendering them from this endpoint prevents the
 * phone preview and AMOLED from showing different typeface families. */
static esp_err_t handle_clock_font_bitmaps(httpd_req_t *req)
{
    cJSON *payload=cJSON_CreateObject();
    if(!payload)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
    cJSON_AddNumberToObject(payload,"width",LF_OPEN_FACE_W);
    cJSON_AddNumberToObject(payload,"height",LF_OPEN_FACE_H);
    cJSON_AddNumberToObject(payload,"families",LF_OPEN_FACE_FAMILIES);
    cJSON_AddStringToObject(payload,"digits",lf_open_face_b64);
    httpd_resp_set_hdr(req,"Cache-Control","max-age=3600");
    return send_json(req,payload);
}
static esp_err_t handle_power_cached(httpd_req_t *req)
{
    lf_power_state_t battery={0};
    const bool valid=lf_power_cached(&battery);
    cJSON *root=cJSON_CreateObject();
    if(!root)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
    cJSON_AddBoolToObject(root,"valid",valid);
    if(valid){
        cJSON_AddNumberToObject(root,"percentage",battery.percentage);
        cJSON_AddBoolToObject(root,"charging",battery.charging);
        cJSON_AddBoolToObject(root,"usb_connected",battery.usb_connected);
    }
    return send_json(req,root);
}

static esp_err_t handle_settings_page(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, lf_basic_settings_html, HTTPD_RESP_USE_STRLEN);
}

/* Weather has its own page and its own NVS write path. Selecting a
 * geocoding result persists exact coordinates for mountain resort names. */
static esp_err_t handle_weather_page(httpd_req_t *req)
{
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_send(req,lf_weather_settings_html,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t handle_weather_get(httpd_req_t *req)
{
    lf_weather_config_t cfg;
    lf_weather_get_config(&cfg);
    char strip[80]={0};
    lf_weather_get_strip(strip,sizeof strip);
    cJSON *root=cJSON_CreateObject();
    if(!root)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
    cJSON_AddBoolToObject(root,"enabled",cfg.enabled);
    cJSON_AddStringToObject(root,"city",cfg.city);
    cJSON_AddNumberToObject(root,"interval_minutes",cfg.interval_minutes);
    cJSON_AddBoolToObject(root,"has_coordinates",cfg.has_coordinates);
    cJSON_AddNumberToObject(root,"latitude_e5",cfg.has_coordinates?cfg.latitude_e5:0);
    cJSON_AddNumberToObject(root,"longitude_e5",cfg.has_coordinates?cfg.longitude_e5:0);
    cJSON_AddStringToObject(root,"forecast",strip);
    return send_json(req,root);
}
static esp_err_t handle_weather_post(httpd_req_t *req)
{
    if(req->content_len<2 || req->content_len>480)
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid weather request length");
    char body[481]={0};
    int total=0,retries=0;
    while(total<req->content_len){
        int n=httpd_req_recv(req,body+total,req->content_len-total);
        if(n==HTTPD_SOCK_ERR_TIMEOUT && ++retries<4)continue;
        if(n<=0)return httpd_resp_send_err(req,HTTPD_408_REQ_TIMEOUT,"Weather request timeout");
        total+=n;
        retries=0;
    }
    cJSON *json=cJSON_Parse(body);
    const cJSON *city=json?cJSON_GetObjectItemCaseSensitive(json,"city"):NULL;
    const cJSON *enabled=json?cJSON_GetObjectItemCaseSensitive(json,"enabled"):NULL;
    const cJSON *exact=json?cJSON_GetObjectItemCaseSensitive(json,"has_coordinates"):NULL;
    const cJSON *interval=json?cJSON_GetObjectItemCaseSensitive(json,"interval_minutes"):NULL;
    const cJSON *lat=json?cJSON_GetObjectItemCaseSensitive(json,"latitude_e5"):NULL;
    const cJSON *lon=json?cJSON_GetObjectItemCaseSensitive(json,"longitude_e5"):NULL;
    bool valid=cJSON_IsObject(json)&&cJSON_IsString(city)&&city->valuestring &&
        strlen(city->valuestring)>0 && strlen(city->valuestring)<=LF_WEATHER_CITY_MAX &&
        cJSON_IsBool(enabled)&&cJSON_IsBool(exact)&&cJSON_IsNumber(interval)&&
        (interval->valueint==15||interval->valueint==30||
         interval->valueint==60||interval->valueint==120)&&
        interval->valuedouble==(double)interval->valueint;
    if(valid){
        for(const char *p=city->valuestring;*p;p++)
            if((unsigned char)*p<32)valid=false;
    }
    const bool has=valid&&cJSON_IsTrue(exact);
    if(has)valid=cJSON_IsNumber(lat)&&cJSON_IsNumber(lon)&&
        lat->valuedouble==(double)lat->valueint &&
        lon->valuedouble==(double)lon->valueint &&
        lat->valueint>=-9000000 && lat->valueint<=9000000 &&
        lon->valueint>=-18000000 && lon->valueint<=18000000;
    if(!valid){
        cJSON_Delete(json);
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid weather settings");
    }
    lf_weather_config_t cfg={0};
    cfg.enabled=cJSON_IsTrue(enabled);
    cfg.interval_minutes=(uint16_t)interval->valueint;
    strlcpy(cfg.city,city->valuestring,sizeof cfg.city);
    cfg.has_coordinates=has;
    if(has){
        cfg.latitude_e5=lat->valueint;
        cfg.longitude_e5=lon->valueint;
    }
    cJSON_Delete(json);
    if(!lf_weather_save_config(&cfg))
        return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Could not save weather");
    cJSON *ack=cJSON_CreateObject();
    cJSON_AddBoolToObject(ack,"saved",true);
    return send_json(req,ack);
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
        cJSON_AddNumberToObject(p,"time_color",s.face_time_color[i]);
        cJSON_AddNumberToObject(p,"battery_color",s.face_battery_color[i]);
        cJSON_AddNumberToObject(p,"weather_color",s.face_weather_color[i]);
        cJSON_AddNumberToObject(p,"date_color",s.face_date_color[i]);
        cJSON_AddBoolToObject(p,"show_date",s.face_show_date[i]!=0);
        cJSON_AddBoolToObject(p,"show_seconds",s.face_show_seconds[i]!=0);
        cJSON_AddBoolToObject(p,"show_battery",s.face_show_battery[i]!=0);
        cJSON_AddNumberToObject(p,"battery_x",s.face_battery_x[i]);
        cJSON_AddNumberToObject(p,"battery_y",s.face_battery_y[i]);
        cJSON_AddNumberToObject(p,"battery_size",s.face_battery_size[i]);
        cJSON_AddBoolToObject(p,"show_percent",s.face_show_percent[i]!=0);
        cJSON_AddBoolToObject(p,"show_icon",s.face_show_icon[i]!=0);
        cJSON_AddNumberToObject(p,"date_format",s.face_date_format[i]);
        cJSON_AddBoolToObject(p,"show_weather",s.face_show_weather[i]!=0);
        cJSON_AddNumberToObject(p,"weather_x",s.face_weather_x[i]);
        cJSON_AddNumberToObject(p,"weather_y",s.face_weather_y[i]);
        cJSON_AddNumberToObject(p,"weather_size",s.face_weather_size[i]);
        cJSON_AddNumberToObject(p,"date_x",s.face_date_x[i]);
        cJSON_AddNumberToObject(p,"date_y",s.face_date_y[i]);
        cJSON_AddNumberToObject(p,"date_size",s.face_date_size[i]);
        cJSON_AddBoolToObject(p,"time_24h",s.face_24h[i]!=0);
        cJSON_AddItemToArray(faces,p);
    }
    cJSON_AddBoolToObject(root, "weather_enabled", weather.enabled);
    cJSON_AddNumberToObject(root, "font_style", s.basic_font_style);
    cJSON_AddNumberToObject(root, "font_size", s.basic_font_size);
    cJSON_AddNumberToObject(root, "basic_time_x", s.basic_time_x);
    cJSON_AddNumberToObject(root, "basic_time_y", s.basic_time_y);
    cJSON_AddNumberToObject(root, "basic_battery_x", s.basic_battery_x);
    cJSON_AddNumberToObject(root, "basic_battery_y", s.basic_battery_y);
    cJSON_AddNumberToObject(root, "basic_date_x", s.basic_date_x);
    cJSON_AddNumberToObject(root, "basic_date_y", s.basic_date_y);
    cJSON_AddNumberToObject(root, "basic_weather_x", s.basic_weather_x);
    cJSON_AddNumberToObject(root, "basic_weather_y", s.basic_weather_y);
    cJSON_AddNumberToObject(root, "basic_battery_size", s.basic_battery_size);
    cJSON_AddNumberToObject(root, "basic_date_size", s.basic_date_size);
    cJSON_AddNumberToObject(root, "basic_weather_size", s.basic_weather_size);
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
    if (req->content_len < 2 || req->content_len > 6500) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request length");
        return ESP_FAIL;
    }
    /* Multiple independent per-face colours exceed the old 3500-byte POST.
       Allocate only the bounded request to avoid overflowing HTTP task stack. */
    char *json=malloc((size_t)req->content_len+1U);
    if(!json)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory for clock settings");
    int received = 0, retries = 0;
    while (received < req->content_len) {
        int n = httpd_req_recv(req, json + received, req->content_len - received);
        if (n == HTTPD_SOCK_ERR_TIMEOUT && ++retries < 4) continue;
        if (n <= 0) {
            free(json);
            lf_diag_error("settings", "Web request receive timeout/disconnect");
            httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Settings request timed out");
            return ESP_FAIL;
        }
        retries = 0;
        received += n;
    }
    json[received] = 0;
    cJSON *root = cJSON_Parse(json);
    free(json);
    lf_settings_t current;
    lf_weather_config_t weather;
    lf_weather_get_config(&weather);
    if (!root || !cJSON_IsObject(root) || lf_settings_load(&current) != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid settings JSON");
        return ESP_FAIL;
    }
    int face=0, font_style=0, font_size=0, ring_mode=0, bg=0, fg=0, a=0, b=0, c=0, interval=0;
    int element[11]={0};
    uint8_t show_date=0, basic_weather=0, weather_enabled=0;
    const cJSON *city = cJSON_GetObjectItemCaseSensitive(root, "city");
    bool valid =
        number_in_range(root, "clock_face", 0, 5, &face) &&
        boolean_value(root, "weather_enabled", &weather_enabled) &&
        number_in_range(root, "font_style", 0, 39, &font_style) &&
        number_in_range(root, "font_size", 0, 200, &font_size) &&
        number_in_range(root, "ring_mode", 0, 2, &ring_mode) &&
        number_in_range(root,"basic_time_x",-100,100,&element[0]) &&
        number_in_range(root,"basic_time_y",-150,150,&element[1]) &&
        number_in_range(root,"basic_battery_x",-180,180,&element[2]) &&
        number_in_range(root,"basic_battery_y",-180,180,&element[3]) &&
        number_in_range(root,"basic_date_x",-180,180,&element[4]) &&
        number_in_range(root,"basic_date_y",-180,180,&element[5]) &&
        number_in_range(root,"basic_weather_x",-180,180,&element[6]) &&
        number_in_range(root,"basic_weather_y",-180,180,&element[7]) &&
        number_in_range(root,"basic_battery_size",0,200,&element[8]) &&
        number_in_range(root,"basic_date_size",0,200,&element[9]) &&
        number_in_range(root,"basic_weather_size",0,200,&element[10]) &&
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
    uint32_t time_face[6],battery_face[6],weather_colour_face[6],date_colour_face[6];
    uint8_t dt_face[6],sc_face[6],bt_face[6],hr_face[6];
    int32_t battery_x_face[6],battery_y_face[6];
    int32_t date_x_face[6],date_y_face[6];
    uint8_t battery_size_face[6],date_size_face[6],percent_face[6];
    uint8_t icon_face[6],date_format_face[6],weather_face[6],weather_size_face[6];
    int32_t weather_x_face[6],weather_y_face[6];
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
        /* Accept older clients without the two new coordinates; preserve
           stored locations. New Clock Studio always sends both fields. */
        int btx=current.face_battery_x[i],bty=current.face_battery_y[i];
        const cJSON *batteryX=cJSON_GetObjectItemCaseSensitive(p,"battery_x");
        const cJSON *batteryY=cJSON_GetObjectItemCaseSensitive(p,"battery_y");
        if((batteryX&&!number_in_range(p,"battery_x",-150,150,&btx)) ||
           (batteryY&&!number_in_range(p,"battery_y",-180,180,&bty))){
            valid=false;break;
        }
        battery_x_face[i]=btx;battery_y_face[i]=bty;
        /* Optional for older local browser pages: absent fields preserve the
           independent saved colours rather than overwriting with accents. */
        int tc=(int)current.face_time_color[i],bc=(int)current.face_battery_color[i];
        int wc=(int)current.face_weather_color[i],dc=(int)current.face_date_color[i];
        const cJSON *tf=cJSON_GetObjectItemCaseSensitive(p,"time_color");
        const cJSON *bf=cJSON_GetObjectItemCaseSensitive(p,"battery_color");
        const cJSON *wf=cJSON_GetObjectItemCaseSensitive(p,"weather_color");
        const cJSON *df=cJSON_GetObjectItemCaseSensitive(p,"date_color");
        if((tf&&!number_in_range(p,"time_color",0,0xFFFFFF,&tc)) ||
           (bf&&!number_in_range(p,"battery_color",0,0xFFFFFF,&bc)) ||
           (wf&&!number_in_range(p,"weather_color",0,0xFFFFFF,&wc)) ||
           (df&&!number_in_range(p,"date_color",0,0xFFFFFF,&dc))){valid=false;break;}
        time_face[i]=(uint32_t)tc;battery_face[i]=(uint32_t)bc;
        weather_colour_face[i]=(uint32_t)wc;date_colour_face[i]=(uint32_t)dc;
        /* Optional for older local clients; never reset saved new values. */
        int bsize=current.face_battery_size[i],dsize=current.face_date_size[i];
        int dx=current.face_date_x[i],dy=current.face_date_y[i];
        uint8_t pct=current.face_show_percent[i];
        uint8_t ico=current.face_show_icon[i],wvis=current.face_show_weather[i];
        int datefmt=current.face_date_format[i],wsz=current.face_weather_size[i];
        int wx=current.face_weather_x[i],wy=current.face_weather_y[i];
        const cJSON *bs=cJSON_GetObjectItemCaseSensitive(p,"battery_size");
        const cJSON *ds=cJSON_GetObjectItemCaseSensitive(p,"date_size");
        const cJSON *x=cJSON_GetObjectItemCaseSensitive(p,"date_x");
        const cJSON *y=cJSON_GetObjectItemCaseSensitive(p,"date_y");
        const cJSON *show=cJSON_GetObjectItemCaseSensitive(p,"show_percent");
        const cJSON *ico_field=cJSON_GetObjectItemCaseSensitive(p,"show_icon");
        const cJSON *df_field=cJSON_GetObjectItemCaseSensitive(p,"date_format");
        const cJSON *wv_field=cJSON_GetObjectItemCaseSensitive(p,"show_weather");
        const cJSON *wx_field=cJSON_GetObjectItemCaseSensitive(p,"weather_x");
        const cJSON *wy_field=cJSON_GetObjectItemCaseSensitive(p,"weather_y");
        const cJSON *wsz_field=cJSON_GetObjectItemCaseSensitive(p,"weather_size");
        if((bs&&!number_in_range(p,"battery_size",0,200,&bsize))||
           (ds&&!number_in_range(p,"date_size",0,200,&dsize))||
           (x&&!number_in_range(p,"date_x",-150,150,&dx))||
           (y&&!number_in_range(p,"date_y",-180,180,&dy))||
           (show&&!boolean_value(p,"show_percent",&pct))||
           (ico_field&&!boolean_value(p,"show_icon",&ico))||
           (df_field&&!number_in_range(p,"date_format",0,5,&datefmt))||
           (wv_field&&!boolean_value(p,"show_weather",&wvis))||
           (wx_field&&!number_in_range(p,"weather_x",-150,150,&wx))||
           (wy_field&&!number_in_range(p,"weather_y",-180,180,&wy))||
           (wsz_field&&!number_in_range(p,"weather_size",0,200,&wsz))){valid=false;break;}
        battery_size_face[i]=(uint8_t)bsize;
        date_size_face[i]=(uint8_t)dsize;
        date_x_face[i]=dx;date_y_face[i]=dy;percent_face[i]=pct;
        icon_face[i]=ico;date_format_face[i]=(uint8_t)datefmt;
        weather_face[i]=wvis;weather_size_face[i]=(uint8_t)wsz;
        weather_x_face[i]=wx;weather_y_face[i]=wy;
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
    (void)face;
    current.clock_face = 5; /* 0.8.124: single Basic face */
    current.basic_font_style = (uint8_t)font_style;
    current.basic_font_size = (uint8_t)font_size;
    current.basic_time_x=(int32_t)element[0];
    current.basic_time_y=(int32_t)element[1];
    current.basic_battery_x=(int32_t)element[2];
    current.basic_battery_y=(int32_t)element[3];
    current.basic_date_x=(int32_t)element[4];
    current.basic_date_y=(int32_t)element[5];
    current.basic_weather_x=(int32_t)element[6];
    current.basic_weather_y=(int32_t)element[7];
    current.basic_battery_size=(uint8_t)element[8];
    current.basic_date_size=(uint8_t)element[9];
    current.basic_weather_size=(uint8_t)element[10];
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
        current.face_time_color[i]=time_face[i];
        current.face_battery_color[i]=battery_face[i];
        current.face_weather_color[i]=weather_colour_face[i];
        current.face_date_color[i]=date_colour_face[i];
        current.face_show_seconds[i]=sc_face[i];current.face_show_battery[i]=bt_face[i];
        current.face_24h[i]=hr_face[i];
        current.face_battery_x[i]=battery_x_face[i];
        current.face_battery_y[i]=battery_y_face[i];
        current.face_battery_size[i]=battery_size_face[i];
        current.face_show_percent[i]=percent_face[i];
        current.face_show_icon[i]=icon_face[i];
        current.face_date_format[i]=date_format_face[i];
        current.face_show_weather[i]=weather_face[i];
        current.face_weather_x[i]=weather_x_face[i];
        current.face_weather_y[i]=weather_y_face[i];
        current.face_weather_size[i]=weather_size_face[i];
        current.face_date_x[i]=date_x_face[i];
        current.face_date_y[i]=date_y_face[i];
        current.face_date_size[i]=date_size_face[i];
    }
    /* Basic face's original editable geometry remains synchronized with the
       same face-local position/size fields for reliable firmware rollback. */
    current.basic_battery_x=current.face_battery_x[5];
    current.basic_battery_y=current.face_battery_y[5];
    current.basic_battery_size=current.face_battery_size[5];
    current.basic_date_x=current.face_date_x[5];
    current.basic_date_y=current.face_date_y[5];
    current.basic_date_size=current.face_date_size[5];
    current.basic_weather=current.face_show_weather[5];
    current.basic_weather_x=current.face_weather_x[5];
    current.basic_weather_y=current.face_weather_y[5];
    current.basic_weather_size=current.face_weather_size[5];
    /* Basic face keeps existing ring/font controls as well. */
    current.face_background[5]=0x000000;
    current.basic_background=0x000000;
    current.basic_foreground=current.face_foreground[5];
    current.basic_show_date=current.face_show_date[5];
    /* Weather is independently managed at /weather. Clock Studio may
       still submit its hidden legacy fields, but must NEVER revert a newer
       Weather page selection. Only explicit legacy clients opting in can
       update weather together with clocks. */
    const bool update_weather=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root,"update_weather"));
    if(update_weather) {
        if(strcmp(weather.city,city->valuestring)!=0){
            weather.has_coordinates=false;
            weather.latitude_e5=0;
            weather.longitude_e5=0;
        }
        strlcpy(weather.city, city->valuestring, sizeof weather.city);
        weather.interval_minutes = (uint16_t)interval;
        weather.enabled = weather_enabled != 0;
    }
    cJSON_Delete(root);
    if(update_weather && !lf_weather_save_config(&weather)) {
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

/* Explicit on-demand I2C presence test: never starts audio sampling. */
static esp_err_t handle_mic_check(httpd_req_t *req)
{
    lf_mic_probe_result_t result=lf_mic_probe_run();
    lf_mic_probe_snapshot_t sample={0};
    lf_mic_probe_snapshot(&sample);
    cJSON *reply=cJSON_CreateObject();
    if(!reply) return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
    cJSON_AddBoolToObject(reply,"adc_detected",result==LF_MIC_PRESENT);
    cJSON_AddStringToObject(reply,"status",lf_mic_probe_result_text(result));
    cJSON_AddNumberToObject(reply,"error_code",sample.error_code);
    cJSON_AddNumberToObject(reply,"test_count",sample.attempts);
    cJSON_AddBoolToObject(reply,"audio_capture_active",false);
    cJSON_AddStringToObject(reply,"detail",result==LF_MIC_PRESENT
        ? "ES7210 detected at I2C 0x40. Audio capture NOT tested or activated."
        : "ES7210 not confirmed. Check board and retry. No audio captured.");
    return send_json(req,reply);
}

/* Manual AudD song recognition. The local GET never reveals the stored key.
 * Each mutation requires an explicit custom header to defeat cross-site POSTs. */
static bool manual_audio_header(httpd_req_t *req)
{
    char marker[8]={0};
    return httpd_req_get_hdr_value_str(req,"X-LyricFlash-Manual-Test",
        marker,sizeof marker)==ESP_OK && strcmp(marker,"1")==0;
}
static esp_err_t handle_song_status(httpd_req_t *req)
{
    lf_recog_snapshot_t s={0};
    lf_recog_snapshot(&s);
    cJSON *reply=cJSON_CreateObject();
    if(!reply)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
    cJSON_AddStringToObject(reply,"state",lf_recog_state_name(s.state));
    cJSON_AddBoolToObject(reply,"token_configured",lf_recog_has_token());
    cJSON_AddStringToObject(reply,"message",s.message);
    cJSON_AddStringToObject(reply,"firmware_version",LF_VERSION);
    cJSON_AddStringToObject(reply,"transport_phase",s.transport_phase);
    cJSON_AddStringToObject(reply,"title",s.title);
    cJSON_AddStringToObject(reply,"artist",s.artist);
    cJSON_AddNumberToObject(reply,"captured_frames",s.captured_frames);
    cJSON_AddNumberToObject(reply,"waveform_level",s.capture_visual_level);
    cJSON_AddNumberToObject(reply,"waveform_signed_level",s.capture_visual_signed);
    cJSON_AddNumberToObject(reply,"capture_seconds",s.capture_seconds);
    cJSON_AddNumberToObject(reply,"attempts",s.attempts);
    cJSON_AddBoolToObject(reply,"worker_busy",lf_recog_busy());
    const uint64_t monotonic_ms=(uint64_t)(esp_timer_get_time()/1000LL);
    cJSON_AddNumberToObject(reply,"stage_age_ms",
        s.stage_started_ms && monotonic_ms>=s.stage_started_ms?
        (double)(monotonic_ms-s.stage_started_ms):0);
    cJSON_AddNumberToObject(reply,"upload_bytes_sent",s.upload_bytes_sent);
    cJSON_AddNumberToObject(reply,"upload_progress_age_ms",
        s.upload_progress_ms && monotonic_ms>=s.upload_progress_ms?
        (double)(monotonic_ms-s.upload_progress_ms):0);
    cJSON_AddNumberToObject(reply,"upload_bytes_total",s.upload_bytes_total);
    /* Safe cross-thread diagnostics: no display lock, no audio or token. */
    lf_ui_display_stats_t live={0};
    lf_ui_display_stats(&live);
    cJSON_AddNumberToObject(reply,"ui_tick_age_ms",live.ui_tick_age_ms);
    cJSON_AddNumberToObject(reply,"record_cancel_phase",live.record_cancel_phase);
    cJSON_AddNumberToObject(reply,"record_cancel_age_ms",live.last_record_cancel_age_ms);
    cJSON_AddNumberToObject(reply,"ui_page",live.ui_page);
    cJSON_AddNumberToObject(reply,"ui_navigation_phase",live.ui_navigation_phase);
    cJSON_AddNumberToObject(reply,"lyrics_auto_open_queued",
                            live.lyrics_auto_open_queued);
    cJSON_AddNumberToObject(reply,"lyrics_auto_open_completed",
                            live.lyrics_auto_open_completed);
    cJSON_AddNumberToObject(reply,"lyrics_auto_open_recoveries",
                            live.lyrics_auto_open_recoveries);
    cJSON_AddNumberToObject(reply,"lyrics_scene_phase",live.lyrics_scene_phase);
    /* The actual submitted stereo scan, not the separate 3-second PCM test.
     * Aggregate metrics only; no audio, credentials or remote response body. */
    cJSON_AddNumberToObject(reply,"audd_http_status",s.http_status);
    cJSON_AddNumberToObject(reply,"audd_api_error_code",s.audd_api_error_code);
    cJSON_AddNumberToObject(reply,"audd_transport_error_code",s.audd_transport_error_code);
    cJSON_AddNumberToObject(reply,"mic_gain_db",27);
    cJSON_AddNumberToObject(reply,"audio_measured_frames",s.audio_measured_frames);
    cJSON_AddNumberToObject(reply,"mic1_peak",s.mic1_peak);
    cJSON_AddNumberToObject(reply,"mic2_peak",s.mic2_peak);
    cJSON_AddNumberToObject(reply,"mic1_rms",s.mic1_rms);
    cJSON_AddNumberToObject(reply,"mic2_rms",s.mic2_rms);
    cJSON_AddNumberToObject(reply,"mic1_rms_without_rail",s.mic1_rms_without_rail);
    cJSON_AddNumberToObject(reply,"mic2_rms_without_rail",s.mic2_rms_without_rail);
    cJSON_AddNumberToObject(reply,"mic1_near_rail",s.mic1_near_rail);
    cJSON_AddNumberToObject(reply,"mic2_near_rail",s.mic2_near_rail);
    cJSON_AddNumberToObject(reply,"mic1_half_scale",s.mic1_half_scale);
    cJSON_AddNumberToObject(reply,"mic2_half_scale",s.mic2_half_scale);
    cJSON_AddNumberToObject(reply,"mic1_rail_bursts",s.mic1_rail_bursts);
    cJSON_AddNumberToObject(reply,"mic2_rail_bursts",s.mic2_rail_bursts);
    cJSON_AddNumberToObject(reply,"mic1_longest_rail_burst",s.mic1_longest_rail_burst);
    cJSON_AddNumberToObject(reply,"mic2_longest_rail_burst",s.mic2_longest_rail_burst);
    cJSON_AddNumberToObject(reply,"both_near_rail_frames",s.both_near_rail_frames);
    cJSON_AddStringToObject(reply,"lyrics_state",
         s.lyrics_state[0]?s.lyrics_state:"not_checked");
    cJSON_AddNumberToObject(reply,"lyric_lines",s.lyric_lines);
    /* A bounded, non-executable preview proves that timestamps were parsed.
     * Full synchronized LRC remains PSRAM-backed for the future device UI. */
    cJSON *preview=cJSON_AddArrayToObject(reply,"lyric_preview");
    if(preview && s.lyric_lines && s.state==LF_RECOG_MATCH) {
        const uint32_t n=s.lyric_lines<4U?s.lyric_lines:4U;
        for(uint32_t i=0;i<n;i++) {
            lf_lyric_line_t line={0};
            if(!lf_recog_lyric_line(i,&line))break;
            cJSON *item=cJSON_CreateObject();
            if(!item)break;
            cJSON_AddNumberToObject(item,"start_ms",line.start_ms);
            cJSON_AddStringToObject(item,"text",line.text);
            cJSON_AddItemToArray(preview,item);
        }
    }
    /* Never return credentials, raw audio, files or the AudD response. */
    return send_json(req,reply);
}
static esp_err_t handle_song_token(httpd_req_t *req)
{
    if(!manual_audio_header(req))
        return httpd_resp_send_err(req,HTTPD_403_FORBIDDEN,"Manual action required");
    if(req->content_len<12||req->content_len>300)
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid token request");
    char body[301]={0};
    int received=0;
    while(received<req->content_len){
        int n=httpd_req_recv(req,body+received,req->content_len-received);
        if(n<=0){memset(body,0,sizeof body);return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Incomplete request");}
        received+=n;
    }
    cJSON *root=cJSON_Parse(body);
    memset(body,0,sizeof body);
    const cJSON *token=root?cJSON_GetObjectItemCaseSensitive(root,"token"):NULL;
    const bool valid=cJSON_IsString(token)&&token->valuestring;
    bool saved=false;
    if(valid) saved=token->valuestring[0]?
           lf_recog_save_token(token->valuestring):lf_recog_clear_token();
    /* Clear the temporary parsed JSON credential before cJSON frees it. */
    if(valid){
        volatile char *p=(volatile char *)token->valuestring;
        for(size_t i=0,len=strlen(token->valuestring);i<len;i++)p[i]=0;
    }
    cJSON_Delete(root);
    if(!valid||!saved)
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,
                                    "Could not save token; check its format or active scan");
    cJSON *reply=cJSON_CreateObject();
    if(!reply)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
    cJSON_AddBoolToObject(reply,"configured",lf_recog_has_token());
    return send_json(req,reply);
}
static esp_err_t handle_song_start(httpd_req_t *req)
{
    if(!manual_audio_header(req))
        return httpd_resp_send_err(req,HTTPD_403_FORBIDDEN,"Manual action required");
    if(!lf_recog_has_token())
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Configure an AudD token first");
    if(!lf_network_is_connected())
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Connect to Wi-Fi first");
    if(!lf_recog_start()){
        httpd_resp_set_status(req,"409 Conflict");
        return httpd_resp_sendstr(req,"Microphone already in use or scan unavailable");
    }
    httpd_resp_set_status(req,"202 Accepted");
    return httpd_resp_sendstr(req,"Manual song recognition started");
}
static esp_err_t handle_song_stop(httpd_req_t *req)
{
    if(!manual_audio_header(req))
        return httpd_resp_send_err(req,HTTPD_403_FORBIDDEN,"Manual action required");
    lf_recog_stop();
    return httpd_resp_sendstr(req,"Stop requested");
}

/* On-demand 3s audio amplitude probe; never sends raw PCM to the browser. */
static esp_err_t handle_mic_pcm_status(httpd_req_t *req)
{
    lf_pcm_snapshot_t s={0};
    lf_pcm_snapshot(&s);
    cJSON *reply=cJSON_CreateObject();
    if(!reply)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"No memory");
    cJSON_AddStringToObject(reply,"state",lf_pcm_state_name(s.state));
    cJSON_AddStringToObject(reply,"error",lf_pcm_error_name(s.error));
    cJSON_AddNumberToObject(reply,"attempts",s.attempts);
    cJSON_AddNumberToObject(reply,"sample_count",s.sample_count);
    cJSON_AddNumberToObject(reply,"peak_abs",s.peak_abs);
    cJSON_AddNumberToObject(reply,"mean_abs",s.mean_abs);
    cJSON_AddNumberToObject(reply,"mic1_mean_abs",s.mic1_mean_abs);
    cJSON_AddNumberToObject(reply,"mic1_peak_abs",s.mic1_peak_abs);
    cJSON_AddNumberToObject(reply,"mic2_mean_abs",s.mic2_mean_abs);
    cJSON_AddNumberToObject(reply,"mic2_peak_abs",s.mic2_peak_abs);
    cJSON_AddNumberToObject(reply,"mic1_rms",s.mic1_rms);
    cJSON_AddNumberToObject(reply,"mic2_rms",s.mic2_rms);
    cJSON_AddNumberToObject(reply,"mic1_near_rail",s.mic1_near_rail);
    cJSON_AddNumberToObject(reply,"mic2_near_rail",s.mic2_near_rail);
    cJSON_AddNumberToObject(reply,"mic1_rail_bursts",s.mic1_rail_bursts);
    cJSON_AddNumberToObject(reply,"mic2_rail_bursts",s.mic2_rail_bursts);
    cJSON_AddNumberToObject(reply,"mic1_longest_rail_burst",s.mic1_longest_rail_burst);
    cJSON_AddNumberToObject(reply,"mic2_longest_rail_burst",s.mic2_longest_rail_burst);
    cJSON_AddNumberToObject(reply,"mic1_rms_below_rail",s.mic1_rms_below_rail);
    cJSON_AddNumberToObject(reply,"mic2_rms_below_rail",s.mic2_rms_below_rail);
    cJSON_AddNumberToObject(reply,"mic1_half_scale",s.mic1_half_scale);
    cJSON_AddNumberToObject(reply,"mic2_half_scale",s.mic2_half_scale);
    cJSON_AddNumberToObject(reply,"both_near_rail_frames",s.both_near_rail_frames);
    cJSON_AddNumberToObject(reply,"half_scale_threshold",16384);
    cJSON_AddNumberToObject(reply,"near_rail_threshold",32760);
    cJSON_AddStringToObject(reply,"audio_input_format","24kHz/16bit/4slot_TDM");
    cJSON_AddStringToObject(reply,"physical_channels","MIC1 slot0, MIC2 slot2");
    cJSON_AddNumberToObject(reply,"elapsed_ms",s.elapsed_ms);
    cJSON_AddBoolToObject(reply,"stop_requested",s.stop_requested);
    cJSON_AddBoolToObject(reply,"audio_uploaded",false);
    cJSON_AddBoolToObject(reply,"audio_saved",false);
    return send_json(req,reply);
}
static esp_err_t handle_mic_pcm_start(httpd_req_t *req)
{
    if(lf_recog_busy()){
        httpd_resp_set_status(req,"409 Conflict");
        return httpd_resp_sendstr(req,"Song recognition is already using the microphones");
    }
    /* A non-simple custom header makes cross-origin browser POSTs fail their
       preflight; an unrelated website must never trigger microphone input. */
    char manual[8]={0};
    if(httpd_req_get_hdr_value_str(req,"X-LyricFlash-Manual-Test",
                                  manual,sizeof(manual))!=ESP_OK ||
       strcmp(manual,"1")!=0)
        return httpd_resp_send_err(req,HTTPD_403_FORBIDDEN,
                                   "Manual local UI trigger required");
    if(!lf_pcm_start()) {
        httpd_resp_set_status(req,"409 Conflict");
        return httpd_resp_sendstr(req,"Audio test already active or task unavailable");
    }
    httpd_resp_set_status(req,"202 Accepted");
    return httpd_resp_sendstr(req,"Microphone sound-level test started");
}
static esp_err_t handle_mic_pcm_stop(httpd_req_t *req)
{
    lf_pcm_stop();
    return httpd_resp_sendstr(req,"Stop requested");
}

/* Local, read-only, paginated history of recognized artist/title pairs.
   The legacy NVS lyric entries can be listed without rewriting them. */
static esp_err_t handle_song_database(httpd_req_t *req)
{
    size_t offset=0;
    char query[72]={0},raw[16]={0};
    if(httpd_req_get_url_query_str(req,query,sizeof(query))==ESP_OK &&
       httpd_query_key_value(query,"offset",raw,sizeof(raw))==ESP_OK){
        char *end=NULL;unsigned long value=strtoul(raw,&end,10);
        if(!raw[0]||!end||*end || value>10000U)
            return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid page offset");
        offset=(size_t)value;
    }
    const size_t limit=20;
    lf_song_history_item_t *items=heap_caps_calloc(limit,sizeof(*items),
                                                    MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!items)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,
                                         "Insufficient memory");
    size_t total=0;
    size_t n=lf_song_history_list(items,limit,offset,&total);
    cJSON *root=cJSON_CreateObject();
    cJSON *entries=root?cJSON_AddArrayToObject(root,"songs"):NULL;
    if(!entries){heap_caps_free(items);cJSON_Delete(root);
        return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Cannot list songs");}
    cJSON_AddNumberToObject(root,"total",total);
    cJSON_AddNumberToObject(root,"offset",offset);
    cJSON_AddNumberToObject(root,"page_size",limit);
    for(size_t i=0;i<n;i++){
        cJSON *entry=cJSON_CreateObject();
        if(!entry)break;
        cJSON_AddStringToObject(entry,"title",items[i].title);
        cJSON_AddStringToObject(entry,"artist",items[i].artist);
        cJSON_AddBoolToObject(entry,"lyrics_cached",items[i].lyrics_cached);
        cJSON_AddItemToArray(entries,entry);
    }
    heap_caps_free(items);
    return send_json(req,root);
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
    cJSON_AddStringToObject(root, "cpu_power_profile", lf_power_cpu_profile_state());
    cJSON_AddStringToObject(root, "wifi_radio_state", lf_network_wifi_power_state());
    cJSON_AddNumberToObject(root, "wifi_radio_sleep_cycles", lf_network_radio_sleep_cycles());
    if (d.battery_valid) {
        cJSON_AddNumberToObject(root, "battery_percent", d.battery.percentage);
        cJSON_AddBoolToObject(root, "usb_connected", d.battery.usb_connected);
        cJSON_AddBoolToObject(root, "charging", d.battery.charging);
    }
    /* Electrical presence test only; no audio capture is enabled. */
    lf_mic_probe_snapshot_t mic={0};
    lf_mic_probe_snapshot(&mic);
    lf_pcm_snapshot_t pcm={0};
    lf_pcm_snapshot(&pcm);
    cJSON_AddStringToObject(root, "microphone",
       pcm.state==LF_PCM_RUNNING ? "manual sound-level test active" : "inactive (manual tests only)");
    cJSON_AddStringToObject(root,"microphone_pcm_status",lf_pcm_state_name(pcm.state));
    cJSON_AddStringToObject(root,"microphone_pcm_error",lf_pcm_error_name(pcm.error));
    cJSON_AddNumberToObject(root,"microphone_pcm_samples",pcm.sample_count);
    cJSON_AddNumberToObject(root,"microphone_pcm_peak",pcm.peak_abs);
    cJSON_AddNumberToObject(root,"microphone_pcm_mean_abs",pcm.mean_abs);
    cJSON_AddNumberToObject(root,"microphone_pcm_mic1_mean",pcm.mic1_mean_abs);
    cJSON_AddNumberToObject(root,"microphone_pcm_mic1_peak",pcm.mic1_peak_abs);
    cJSON_AddNumberToObject(root,"microphone_pcm_mic2_mean",pcm.mic2_mean_abs);
    cJSON_AddNumberToObject(root,"microphone_pcm_mic2_peak",pcm.mic2_peak_abs);
    cJSON_AddNumberToObject(root,"microphone_pcm_mic1_rms",pcm.mic1_rms);
    cJSON_AddNumberToObject(root,"microphone_pcm_mic2_rms",pcm.mic2_rms);
    cJSON_AddNumberToObject(root,"microphone_pcm_mic1_near_rail",pcm.mic1_near_rail);
    cJSON_AddNumberToObject(root,"microphone_pcm_mic2_near_rail",pcm.mic2_near_rail);
    cJSON_AddStringToObject(root, "microphone_adc_status", lf_mic_probe_result_text(mic.result));
    cJSON_AddNumberToObject(root, "microphone_adc_test_count", mic.attempts);
    cJSON_AddNumberToObject(root, "microphone_adc_error_code", mic.error_code);
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
    cJSON_AddNumberToObject(root, "ui_last_screen_off_ms", display_stats.last_screen_off_ms);
    cJSON_AddNumberToObject(root, "ui_wake_count", display_stats.wake_count);
    cJSON_AddBoolToObject(root, "ui_screen_awake", display_stats.screen_awake);
    cJSON_AddBoolToObject(root, "ui_wake_overlay_pending", display_stats.wake_overlay_pending);
    if(display_stats.last_wake_age_ms!=UINT32_MAX)
        cJSON_AddNumberToObject(root, "ui_last_wake_age_ms", display_stats.last_wake_age_ms);
    cJSON_AddNumberToObject(root, "wake_recovery_count", d.wake_recovery_count);
    cJSON_AddNumberToObject(root, "wake_recovery_last_reason_code", d.wake_recovery_last_reason);
    cJSON_AddNumberToObject(root, "wake_recovery_last_age_ms", d.wake_recovery_last_age_ms);
    cJSON_AddNumberToObject(root, "wake_recovery_last_tick_age_ms", d.wake_recovery_last_tick_age_ms);
    cJSON_AddNumberToObject(root, "ui_last_wake_frame_ready_ms", display_stats.last_wake_frame_ready_ms);
    cJSON_AddNumberToObject(root, "ui_last_wake_overlay_clear_ms", display_stats.last_wake_overlay_clear_ms);
    cJSON_AddNumberToObject(root, "ui_tick_work_last_us", display_stats.ui_tick_work_last_us);
    cJSON_AddNumberToObject(root, "ui_tick_work_max_us", display_stats.ui_tick_work_max_us);
    cJSON_AddNumberToObject(root, "pager_frame_gap_last_ms", display_stats.pager_frame_gap_last_ms);
    cJSON_AddNumberToObject(root, "pager_frame_gap_max_ms", display_stats.pager_frame_gap_max_ms);
    cJSON_AddNumberToObject(root, "pager_clock_home_mid_draw_last_us", display_stats.pager_clock_home_mid_draw_last_us);
    cJSON_AddNumberToObject(root, "pager_clock_home_mid_draw_max_us", display_stats.pager_clock_home_mid_draw_max_us);
    cJSON_AddNumberToObject(root, "pager_clock_home_mid_draw_samples", display_stats.pager_clock_home_mid_draw_samples);
    cJSON_AddNumberToObject(root, "pager_home_settings_mid_draw_last_us", display_stats.pager_home_settings_mid_draw_last_us);
    cJSON_AddNumberToObject(root, "pager_home_settings_mid_draw_max_us", display_stats.pager_home_settings_mid_draw_max_us);
    cJSON_AddNumberToObject(root, "pager_home_settings_mid_draw_samples", display_stats.pager_home_settings_mid_draw_samples);
    cJSON_AddNumberToObject(root, "render_draw_last_us", display_stats.render_draw_last_us);
    cJSON_AddNumberToObject(root, "render_draw_max_us", display_stats.render_draw_max_us);
    cJSON_AddNumberToObject(root, "flush_wait_refresh_last_us", display_stats.flush_wait_refresh_last_us);
    cJSON_AddNumberToObject(root, "flush_wait_refresh_max_us", display_stats.flush_wait_refresh_max_us);
    cJSON_AddNumberToObject(root, "flush_calls_refresh_last", display_stats.flush_calls_refresh_last);
    cJSON_AddNumberToObject(root, "flush_calls_refresh_max", display_stats.flush_calls_refresh_max);
    cJSON_AddNumberToObject(root, "render_dma_stage_last_us", display_stats.render_dma_stage_last_us);
    cJSON_AddNumberToObject(root, "render_dma_stage_max_us", display_stats.render_dma_stage_max_us);
    bsp_display_transfer_stats_t transfer_stats={0};
    bsp_display_transfer_stats(&transfer_stats);
    cJSON_AddNumberToObject(root,"display_dma_stage_rows",transfer_stats.stage_rows);
    cJSON_AddNumberToObject(root,"display_dma_chunk_submissions",transfer_stats.chunk_submissions);
    cJSON_AddNumberToObject(root,"display_dma_stage_last_us",transfer_stats.stage_call_last_us);
    cJSON_AddNumberToObject(root,"display_dma_stage_max_us",transfer_stats.stage_call_max_us);
    cJSON_AddNumberToObject(root,"free_internal_dma_bytes",
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(root,"largest_internal_dma_block_bytes",
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(root, "ui_navigation_commits", display_stats.navigation_commits);
    cJSON_AddNumberToObject(root, "ui_full_redraws", display_stats.full_redraws);
    cJSON_AddNumberToObject(root, "record_cancel_phase", display_stats.record_cancel_phase);
    cJSON_AddNumberToObject(root, "record_cancel_age_ms", display_stats.last_record_cancel_age_ms);
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
    cJSON_AddNumberToObject(root, "nav_reveal_completions", display_stats.nav_reveal_completions);
    cJSON_AddNumberToObject(root, "nav_reveal_last_ms", display_stats.nav_reveal_last_ms);
    cJSON_AddNumberToObject(root, "pager_forced_snaps", display_stats.pager_forced_snaps);
    cJSON_AddNumberToObject(root, "pager_snap_last_ms", display_stats.pager_snap_last_ms);
    cJSON_AddNumberToObject(root, "pager_snap_max_ms", display_stats.pager_snap_max_ms);
    cJSON_AddNumberToObject(root, "pager_final_frame_acks", display_stats.pager_final_frame_acks);
    cJSON_AddNumberToObject(root, "pager_final_frame_repaint_requests", display_stats.pager_final_frame_repaint_requests);
    cJSON_AddNumberToObject(root, "pager_acknowledged_settles_no_full_repaint", display_stats.pager_acknowledged_settles_no_full_repaint);
    cJSON_AddNumberToObject(root, "render_cycle_last_ms", display_stats.render_cycle_last_ms);
    cJSON_AddNumberToObject(root, "render_cycle_max_ms", display_stats.render_cycle_max_ms);
    cJSON_AddNumberToObject(root, "clock_repaint_recoveries", display_stats.clock_repaint_recoveries);
    {
        lf_ui_bench_t bench={0};
        lf_ui_bench(&bench);
        bsp_display_profile_t lcd={0};
        bsp_display_profile(&lcd);
        cJSON_AddNumberToObject(root, "bench_cpu_mhz", bench.cpu_mhz_measured);
        cJSON_AddNumberToObject(root, "bench_cpu_loop_us", bench.cpu_loop_us);
        cJSON_AddNumberToObject(root, "bench_blank_frame_ms", bench.blank_frame_ms);
        cJSON_AddNumberToObject(root, "bench_clock_frame_ms", bench.page_frame_ms[0]);
        cJSON_AddNumberToObject(root, "bench_home_frame_ms", bench.page_frame_ms[1]);
        cJSON_AddNumberToObject(root, "bench_settings_frame_ms", bench.page_frame_ms[2]);
        cJSON_AddNumberToObject(root, "bench_mid_clock_home_ms", bench.mid_frame_ms[0]);
        cJSON_AddNumberToObject(root, "bench_mid_home_settings_ms", bench.mid_frame_ms[1]);
        cJSON_AddNumberToObject(root, "render_cpu_mhz_min", bench.render_cpu_mhz_min);
        cJSON_AddNumberToObject(root, "render_cpu_mhz_last", bench.render_cpu_mhz_last);
        cJSON_AddBoolToObject(root, "lcd_buffers_internal", lcd.lvgl_buffers_internal);
        cJSON_AddNumberToObject(root, "lcd_buffer_rows", lcd.lvgl_buffer_rows);
        cJSON_AddNumberToObject(root, "lcd_internal_free_at_init", lcd.internal_free_at_init);
        cJSON_AddNumberToObject(root, "lcd_selftest_kbps", lcd.selftest_kbytes_per_s);
        cJSON_AddNumberToObject(root, "lcd_selftest_frame_us", lcd.selftest_frame_us);
        cJSON_AddNumberToObject(root, "free_internal_bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        cJSON_AddNumberToObject(root, "min_free_internal_bytes", heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
        lf_ui_sleep_info_t sl={0};
        lf_ui_sleep_info(&sl);
        cJSON_AddNumberToObject(root, "screen_off_count", sl.count);
        cJSON_AddNumberToObject(root, "screen_off_timeout_s", sl.sleep_seconds);
        cJSON_AddNumberToObject(root, "screen_off_idle_ms", sl.idle_ms);
        cJSON_AddNumberToObject(root, "screen_off_lvgl_idle_ms", sl.lvgl_idle_ms);
        cJSON_AddNumberToObject(root, "screen_off_page", sl.page);
        cJSON_AddNumberToObject(root, "screen_off_at_uptime_s", sl.uptime_s);
        lf_swipe_log_t sw;
        lf_ui_swipe_log(&sw);
        cJSON_AddNumberToObject(root, "swipes_total", sw.total);
        lf_ui_input_state_t in={0};
        lf_ui_input_state(&in);
        cJSON_AddBoolToObject(root, "ui_compositor_active", in.snap_mode);
        cJSON_AddBoolToObject(root, "ui_pager_scrolling", in.pager_scrolling);
        cJSON_AddBoolToObject(root, "ui_pager_snapping", in.pager_snapping);
        cJSON_AddBoolToObject(root, "ui_touch_held", in.touch_held);
        cJSON_AddBoolToObject(root, "ui_drawing_enabled", in.invalidation_enabled);
        cJSON_AddBoolToObject(root, "ui_touch_pressed_now", in.indev_pressed);
        cJSON_AddNumberToObject(root, "ui_last_touch_age_ms", in.lvgl_idle_ms);
        cJSON_AddNumberToObject(root, "ui_page_id", in.page);
        cJSON_AddNumberToObject(root, "lcd_te_pulses", bsp_display_te_count());
        bsp_input_health_t ih={0};
        bsp_display_input_health(&ih);
        cJSON_AddNumberToObject(root, "touch_reads", ih.touch_reads);
        cJSON_AddNumberToObject(root, "touch_read_failures", ih.touch_failures);
        cJSON_AddNumberToObject(root, "touch_read_max_us", ih.touch_read_max_us);
        cJSON_AddNumberToObject(root, "touch_sample_age_ms", ih.touch_sample_age_ms);
        cJSON_AddNumberToObject(root, "lvgl_loop_age_ms", ih.lvgl_loop_age_ms);
        cJSON_AddNumberToObject(root, "ui_stage_now", bsp_ui_stage_get());
        cJSON_AddNumberToObject(root, "ui_stage_at_last_restart", bsp_ui_stage_prev_boot());
        cJSON_AddNumberToObject(root, "lcd_wait_timeouts", bsp_display_lcd_wait_timeouts());
        cJSON_AddBoolToObject(root, "lcd_bus_wedged", bsp_display_spi_wedged());
        lf_weather_debug_t wd;
        lf_weather_debug(&wd);
        lf_settings_t *ws=calloc(1,sizeof(*ws));
        if(ws)(void)lf_settings_load(ws);
        cJSON_AddBoolToObject(root, "weather_enabled_cfg", wd.enabled);
        cJSON_AddStringToObject(root, "weather_city_cfg", wd.city);
        cJSON_AddStringToObject(root, "weather_status", wd.status);
        cJSON_AddBoolToObject(root, "weather_have_reading", wd.valid);
        cJSON_AddNumberToObject(root, "weather_temp_c", wd.temperature);
        cJSON_AddNumberToObject(root, "weather_fetches", wd.fetches);
        cJSON_AddNumberToObject(root, "weather_fetch_ok", wd.fetch_ok);
        cJSON_AddNumberToObject(root, "weather_last_esp_err", wd.last_err);
        cJSON_AddNumberToObject(root, "weather_last_http", wd.last_http);
        cJSON_AddNumberToObject(root, "weather_last_bytes", wd.last_bytes);
        cJSON_AddBoolToObject(root, "weather_last_overflow", wd.last_overflow);
        if(ws){
            cJSON_AddBoolToObject(root, "clock_show_weather", ws->face_show_weather[5]!=0);
            cJSON_AddBoolToObject(root, "basic_weather_menu", ws->basic_weather!=0);
            free(ws);
        }
        uint32_t snap_n=0,snap_last=0,snap_max=0,snap_sw=0;
        lf_ui_snapshot_stats(&snap_n,&snap_last,&snap_max,&snap_sw);
        cJSON_AddNumberToObject(root, "snapshot_swipes", snap_sw);
        cJSON_AddNumberToObject(root, "snapshots_taken", snap_n);
        cJSON_AddNumberToObject(root, "snapshot_last_ms", snap_last);
        cJSON_AddNumberToObject(root, "snapshot_max_ms", snap_max);
        cJSON_AddNumberToObject(root, "snapshot_sync_retakes", lf_ui_snapshot_sync_retakes());
        lf_captive_stats_t portal;
        lf_captive_stats(&portal);
        cJSON_AddBoolToObject(root, "setup_network_active", in_ap);
        cJSON_AddBoolToObject(root, "captive_dns_running", portal.running);
        cJSON_AddNumberToObject(root, "captive_dns_queries", portal.queries);
        cJSON_AddNumberToObject(root, "captive_dns_answered", portal.answered);
        cJSON_AddNumberToObject(root, "captive_redirects", portal.redirects);
        cJSON *arr=cJSON_AddArrayToObject(root, "swipes_recent");
        static const char *names[3]={"clock","home","settings"};
        for(unsigned i=0;arr && i<sw.count;i++){
            const lf_swipe_stat_t *x=&sw.s[i];
            cJSON *o=cJSON_CreateObject();
            if(!o)break;
            cJSON_AddStringToObject(o,"from",x->from_page<3?names[x->from_page]:"?");
            cJSON_AddStringToObject(o,"to",x->to_page<3?names[x->to_page]:"?");
            cJSON_AddNumberToObject(o,"ms",x->duration_ms);
            cJSON_AddNumberToObject(o,"frames",x->frames);
            cJSON_AddNumberToObject(o,"fps",x->duration_ms?(x->frames*1000U)/x->duration_ms:0);
            cJSON_AddNumberToObject(o,"avg_frame_ms",x->avg_gap_ms);
            cJSON_AddNumberToObject(o,"worst_frame_ms",x->max_gap_ms);
            cJSON_AddNumberToObject(o,"avg_draw_ms",x->avg_draw_ms);
            cJSON_AddNumberToObject(o,"worst_draw_ms",x->max_draw_ms);
            cJSON_AddNumberToObject(o,"avg_wait_ms",x->avg_wait_ms);
            cJSON_AddNumberToObject(o,"frames_over_50ms",x->over50);
            cJSON_AddNumberToObject(o,"frames_over_100ms",x->over100);
            cJSON_AddItemToArray(arr,o);
        }
    }
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
    char response[240];
    snprintf(response,sizeof(response),
        "{\"app\":\"LyricFlash\",\"name\":\"LyricFlash 1.75\",\"version\":\"%s\","
        "\"device_id\":\"LF-%02X%02X%02X\",\"mac\":\"%02X:%02X:%02X:%02X:%02X:%02X\"}",
        LF_VERSION,mac[3],mac[4],mac[5],mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
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

/* 1.0.3 captive portal: on the setup network, any page the device does not
   serve (the "am I online?" checks of iOS, macOS, Android and Windows, or a
   typed address) is redirected to the LyricFlash home page, so the setup page
   opens by itself when someone joins. On home Wi-Fi unknown pages stay 404. */
static esp_err_t handle_not_found(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;
    if (!in_ap) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_FAIL;
    }
    char location[40];
    snprintf(location, sizeof location, "http://%s/", address);
    lf_captive_note_redirect();
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", location);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send(req, "LyricFlash setup", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
/* RFC 8908 captive portal API, announced through DHCP option 114. */
static esp_err_t handle_captive_api(httpd_req_t *req)
{
    char body[96];
    if (in_ap)
        snprintf(body, sizeof body,
                 "{\"captive\":true,\"user-portal-url\":\"http://%s/\"}", address);
    else snprintf(body, sizeof body, "{\"captive\":false}");
    httpd_resp_set_type(req, "application/captive+json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, body);
}
static void start_captive_portal(void)
{
    /* 1.0.5: same proven approach as the Basecamp bootloader. The stock
       SoftAP DHCP already names the device as DNS server; restarting DHCP to
       add options (1.0.3/1.0.4) is not needed and is the likely reason the
       portal did not appear, so DHCP is left untouched. */
    esp_netif_t *ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_ip_info_t ip = {0};
    uint32_t addr = 0;
    if (ap && esp_netif_get_ip_info(ap, &ip) == ESP_OK && ip.ip.addr != 0) addr = ip.ip.addr;
    else addr = ESP_IP4TOADDR(192, 168, 4, 1);
    if (!lf_captive_dns_start(addr))
        ESP_LOGW(TAG, "Captive portal DNS did not start");
}
static bool serve(void)
{
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();
    cfg.server_port=80; cfg.stack_size=8192;
    cfg.max_uri_handlers=32; /* 1.0.4: captive portal API route */
    cfg.recv_wait_timeout=15; cfg.send_wait_timeout=15;
    /* 1.0.4: phones probing a captive portal open several connections at
       once; allow more and recycle the oldest idle one instead of refusing. */
    cfg.max_open_sockets=10; cfg.lru_purge_enable=true;
    if(httpd_start(&server,&cfg)!=ESP_OK) return false;
    const httpd_uri_t routes[]={
        {.uri="/",.method=HTTP_GET,.handler=handle_home},
        {.uri="/firmware",.method=HTTP_GET,.handler=handle_firmware_page},
        {.uri="/api/install",.method=HTTP_POST,.handler=handle_github_install},
        {.uri="/api/info",.method=HTTP_GET,.handler=handle_info},
        {.uri="/settings",.method=HTTP_GET,.handler=handle_settings_page},
        {.uri="/weather",.method=HTTP_GET,.handler=handle_weather_page},
        {.uri="/api/weather",.method=HTTP_GET,.handler=handle_weather_get},
        {.uri="/api/weather",.method=HTTP_POST,.handler=handle_weather_post},
        {.uri="/api/power",.method=HTTP_GET,.handler=handle_power_cached},
        {.uri="/api/clock-fonts",.method=HTTP_GET,.handler=handle_clock_font_bitmaps},
        {.uri="/api/preferences",.method=HTTP_GET,.handler=handle_preferences_get},
        {.uri="/api/preferences",.method=HTTP_POST,.handler=handle_preferences_post},
        {.uri="/api/basic",.method=HTTP_GET,.handler=handle_basic_get},
        {.uri="/api/basic",.method=HTTP_POST,.handler=handle_basic_post},
        {.uri="/api/diagnostics",.method=HTTP_GET,.handler=handle_diagnostics},
        {.uri="/api/song-database",.method=HTTP_GET,.handler=handle_song_database},
        {.uri="/api/hardware/mic-check",.method=HTTP_POST,.handler=handle_mic_check},
        {.uri="/api/hardware/mic-test",.method=HTTP_GET,.handler=handle_mic_pcm_status},
        {.uri="/api/hardware/mic-test/start",.method=HTTP_POST,.handler=handle_mic_pcm_start},
        {.uri="/api/hardware/mic-test/stop",.method=HTTP_POST,.handler=handle_mic_pcm_stop},
        {.uri="/api/recognize",.method=HTTP_GET,.handler=handle_song_status},
        {.uri="/api/recognize/token",.method=HTTP_POST,.handler=handle_song_token},
        {.uri="/api/recognize/start",.method=HTTP_POST,.handler=handle_song_start},
        {.uri="/api/recognize/stop",.method=HTTP_POST,.handler=handle_song_stop},
        {.uri="/api/device",.method=HTTP_GET,.handler=handle_device},
        {.uri="/wifi",.method=HTTP_POST,.handler=handle_wifi},
        {.uri="/api/wifi-scan",.method=HTTP_GET,.handler=handle_scan},
        {.uri="/update",.method=HTTP_POST,.handler=handle_update},
        {.uri="/captive",.method=HTTP_GET,.handler=handle_captive_api}
    };
    for(size_t i=0;i<sizeof(routes)/sizeof(routes[0]);i++)
        if(httpd_register_uri_handler(server,&routes[i])!=ESP_OK) return false;
    httpd_register_err_handler(server,HTTPD_404_NOT_FOUND,handle_not_found);
    if(in_ap)start_captive_portal();
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
    /* Fallback to an open setup access point if home Wi-Fi is unavailable. */
    in_ap=true;
    /* 1.0.6: one fixed, easy-to-spot name for the setup network. */
    strlcpy(ap_ssid,"LyricFlash-Setup",sizeof ap_ssid);
    /* 1.0.1: open setup network (no password) so anyone setting the device
       up can join it. Never replace/erase the saved home Wi-Fi credentials. */
    ap_pass[0]=0;
    wifi_config_t apcfg={0};
    strlcpy((char *)apcfg.ap.ssid,ap_ssid,sizeof(apcfg.ap.ssid));
    apcfg.ap.ssid_len=strlen(ap_ssid);
    apcfg.ap.channel=6;
    apcfg.ap.max_connection=4;
    apcfg.ap.authmode=WIFI_AUTH_OPEN;
    if(esp_wifi_set_mode(WIFI_MODE_AP)!=ESP_OK ||
       esp_wifi_set_config(WIFI_IF_AP,&apcfg)!=ESP_OK ||
       esp_wifi_start()!=ESP_OK) return false;
    strlcpy(address,"192.168.4.1",sizeof(address));
    ESP_LOGI(TAG,"Provisioning AP active: %s",ap_ssid);
    return serve();
}
