#include "network.h"
#include "ui.h"
#include "settings.h"
#include "weather.h"
#include "diagnostics.h"
#include "version.h"
#include "web_basic_html.h"
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
static char ota_pin[10];
static uint8_t ota_pin_attempts;
static TickType_t ota_expiry;
static httpd_handle_t server;
static const char *ota_result = "idle";
static uint32_t ota_received;
static uint32_t ota_total;
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
    setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
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
static bool pin_matches(httpd_req_t *req)
{
    char pin[20];
    size_t n=httpd_req_get_hdr_value_len(req,"X-LyricFlash-PIN");
    if(!n || n>=sizeof(pin)) return false;
    if(httpd_req_get_hdr_value_str(req,"X-LyricFlash-PIN",pin,sizeof(pin))!=ESP_OK)
        return false;
    /* No OTA unless explicitly armed from the device touchscreen. */
    if (!lf_network_ota_armed() || ota_pin_attempts >= 5) return false;
    ota_pin_attempts++;
    return strcmp(pin,ota_pin)==0;
}
static void reboot_task(void *arg)
{
    (void)arg; vTaskDelay(pdMS_TO_TICKS(900)); esp_restart();
}
static const char home_page[] =
"<!doctype html><html lang='en'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>LyricFlash Device</title><meta name='theme-color' content='#050910'>"
"<style>*{box-sizing:border-box}body{font:16px system-ui,sans-serif;"
"background:radial-gradient(circle at 15% 0%,#102d56,#050910 52%);"
"color:#f0f6ff;max-width:560px;margin:35px auto;padding:20px;line-height:1.55}"
"h1{letter-spacing:.08em;color:#62b5ff;margin-bottom:0}p{color:#aabbd4}"
"h2{font-size:20px;margin-top:0;color:#eaf5ff}"
"button,input,select{font:inherit;padding:14px;border-radius:12px;width:100%;margin:7px 0}"
"button{background:#62b5ff;border:1px solid #8fc8ff;color:#07182d;font-weight:750;cursor:pointer}"
"button:hover{background:#91ccff}button:focus-visible,input:focus-visible{outline:2px solid #9cd6ff;outline-offset:2px}"
"input,select{background:#0b1525;color:#f0f6ff;border:1px solid #395780}"
"section{background:#101824;border:1px solid #293b58;border-radius:19px;padding:23px;margin:20px 0}"
"#msg{color:#a8d7ff;overflow-wrap:anywhere}</style>"
"<h1>LYRICFLASH</h1><p>ESP32-S3 / Local device manager</p>"
"<p><a href='/settings' style='color:#8ec9ff'>Open Basic clock studio and diagnostics</a></p>"
"<section><h2>Wi-Fi setup</h2><p id='wifi-state'>Checking current network...</p><p>Scan for 2.4 GHz Wi-Fi or enter its name.</p>"
"<button onclick='scan()'>Scan nearby Wi-Fi</button><select id='networks' onchange='pick()'>"
"<option value=''>Choose a network</option></select><input id='ssid' placeholder='Wi-Fi name'>"
"<input id='pass' type='password' placeholder='Wi-Fi password'>"
"<button onclick='save()'>Save Wi-Fi &amp; restart</button></section>"
"<section><h2>Firmware update</h2><p>Tap <b>ARM OTA</b> on the device, then enter the PIN shown on screen.</p>"
"<input id='pin' type='password' placeholder='Device OTA PIN' autocomplete='off'>"
"<input id='bin' type='file' accept='.bin,application/octet-stream'>"
"<button id='install-btn' onclick='update()'>Install application-only OTA firmware</button></section>"
"<p id='msg' role='status'></p><script>const msg=t=>document.getElementById('msg').textContent=t;"
"async function loadState(){try{let r=await fetch('/api/info');let d=await r.json();"
"let state=document.getElementById('wifi-state');state.textContent=d.connected_ssid?"
"('Connected: '+d.connected_ssid):d.saved_ssid?('Saved: '+d.saved_ssid+' (not connected)'):'No home Wi-Fi saved';"
"if(d.saved_ssid)document.getElementById('ssid').value=d.saved_ssid;}catch(e){}}"
"window.addEventListener('DOMContentLoaded',loadState);"
"function pick(){const n=document.getElementById('networks');if(n.value)document.getElementById('ssid').value=n.value;}"
"async function scan(){msg('Scanning Wi-Fi...');try{await fetch('/api/wifi-scan?refresh=1');poll(0);}catch(e){msg('Scan unavailable');}}"
"async function poll(attempt){try{let r=await fetch('/api/wifi-scan');let d=await r.json();if(d.state==='scanning'&&attempt<16){setTimeout(()=>poll(attempt+1),650);return;}"
"let select=document.getElementById('networks');select.replaceChildren(new Option('Choose a network',''));"
"for(let n of d.networks||[])select.add(new Option(n.ssid+(n.secured?' [secured]':' [open]'),n.ssid));"
"msg(d.state==='ready'?(d.networks.length+' networks found'):'Scan failed. Try again.');}catch(e){msg('Scan unavailable');}}"
"async function save(){let r=await fetch('/wifi',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({ssid:document.getElementById('ssid').value,password:document.getElementById('pass').value})});msg(await r.text());}"
"let busy=false;\n"
"function update(){\n"
" const f=document.getElementById('bin').files[0];\n"
" const pin=document.getElementById('pin').value.trim();\n"
" if(!f)return msg('Choose the application-only OTA .bin file first');\n"
" if(!/^[0-9]{4}$/.test(pin))return msg('Enter the four-digit PIN shown on device');\n"
" if(busy)return;\n"
" const button=document.getElementById('install-btn');\n"
" const xhr=new XMLHttpRequest();\n"
" busy=true;button.disabled=true;\n"
" const done=t=>{busy=false;button.disabled=false;msg(t);};\n"
" xhr.open('POST','/update',true);xhr.timeout=180000;\n"
" xhr.setRequestHeader('Content-Type','application/octet-stream');\n"
" xhr.setRequestHeader('X-LyricFlash-PIN',pin);\n"
" xhr.upload.onprogress=e=>msg(e.lengthComputable?'Transfer '+Math.round(100*e.loaded/e.total)+'% (not yet verified)':'Transferring...');\n"
" xhr.upload.onload=()=>msg('Firmware transmitted; waiting for verification...');\n"
" xhr.onload=()=>done(xhr.status>=200&&xhr.status<300?xhr.responseText:'HTTP '+xhr.status+': '+(xhr.responseText||'Upload rejected'));\n"
" xhr.onerror=()=>done('Connection failed. Check device status before retrying.');\n"
" xhr.ontimeout=()=>done('Upload timed out. Check /api/info before retrying.');\n"
" xhr.onabort=()=>done('Upload cancelled.');\n"
" msg('Starting upload...');\n"
" try{xhr.send(f);}catch(e){done('Cannot start upload: '+e.message);}\n"
"}\n"
"</script></html>";

static esp_err_t handle_home(httpd_req_t *req)
{
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    return httpd_resp_send(req,home_page,HTTPD_RESP_USE_STRLEN);
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
    if (!pin_matches(req)) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Arm WEB/OTA on device and enter correct PIN");
        return ESP_FAIL;
    }
    ota_expiry = 0; /* Web configuration authorization is single-use. */
    if (req->content_len < 2 || req->content_len > 1250) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request length");
        return ESP_FAIL;
    }
    char json[1251];
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
    cJSON_AddNumberToObject(root, "ota_received", ota_received);
    cJSON_AddNumberToObject(root, "ota_total", ota_total);
    cJSON_AddNumberToObject(root, "max_ui_tick_gap_ms", lf_ui_max_tick_gap_ms());
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
    cJSON_AddBoolToObject(root, "ota_armed", lf_network_ota_armed());
    cJSON_AddStringToObject(root, "ota_result", ota_result);
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
    if(!in_ap) { httpd_resp_send_err(req,HTTPD_403_FORBIDDEN,"Connect to the device setup AP to change Wi-Fi"); return ESP_FAIL; }
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
    if(!pin_matches(req)){httpd_resp_send_err(req,HTTPD_403_FORBIDDEN,"Arm OTA on device and enter the correct PIN");return ESP_FAIL;}
    ota_expiry=0; /* Single-use PIN; do not require it to remain armed during upload. */
    ota_received=0;
    ota_total=req->content_len>0?(uint32_t)req->content_len:0;
    ota_result="validating";
    const esp_partition_t *part=esp_ota_get_next_update_partition(NULL);
    if(!part || req->content_len<512 || req->content_len>(int)part->size){
        ota_result="bad_size"; ESP_LOGE(TAG,"Invalid OTA request size / inactive partition");
        httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"OTA image invalid or too large");return ESP_FAIL;
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
    ota_expiry=0; // disarm after a single attempt, whether successful or not
    if(!ok){lf_diag_error("ota",ota_result);if(started) esp_ota_abort(handle);ESP_LOGE(TAG,"OTA incomplete at %u/%u",ota_received,ota_total);
        httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Upload failed, device remains on current firmware");return ESP_FAIL;}
    ota_result="verifying";
    esp_err_t finish_err=esp_ota_end(handle);
    if(finish_err!=ESP_OK){ota_result="checksum_failed";lf_diag_error("ota",ota_result);ESP_LOGE(TAG,"OTA end: %s",esp_err_to_name(finish_err));httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Firmware checksum / image validation failed");return ESP_FAIL;}
    esp_err_t boot_err=esp_ota_set_boot_partition(part);
    if(boot_err!=ESP_OK){ota_result="activation_failed";lf_diag_error("ota",ota_result);ESP_LOGE(TAG,"OTA activate: %s",esp_err_to_name(boot_err));httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Cannot activate new firmware");return ESP_FAIL;}
    ota_result="verified_rebooting";
    httpd_resp_sendstr(req,"Firmware verified. Rebooting into new application.");
    xTaskCreate(reboot_task,"lf_reboot",2048,NULL,3,NULL);
    return ESP_OK;
}
static bool serve(void)
{
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();
    cfg.server_port=80; cfg.stack_size=8192;
    cfg.max_uri_handlers=16; /* Web settings/telemetry add routes beyond IDF default. */
    cfg.recv_wait_timeout=15; cfg.send_wait_timeout=15;
    if(httpd_start(&server,&cfg)!=ESP_OK) return false;
    const httpd_uri_t routes[]={
        {.uri="/",.method=HTTP_GET,.handler=handle_home},
        {.uri="/api/info",.method=HTTP_GET,.handler=handle_info},
        {.uri="/settings",.method=HTTP_GET,.handler=handle_settings_page},
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
const char *lf_network_ota_pin(void){return ota_pin;}
void lf_network_arm_ota(void){ota_pin_attempts=0;ota_expiry=xTaskGetTickCount()+pdMS_TO_TICKS(120000);}
bool lf_network_ota_armed(void){return ota_expiry && (int32_t)(ota_expiry-xTaskGetTickCount())>0;}

bool lf_network_start(void)
{
    uint32_t seed=esp_random();
    snprintf(ota_pin,sizeof ota_pin,"%04" PRIu32,seed%10000u);
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
    /* Keep the setup AP credential stable across reboots and OTA updates.
       Never print it to logs; it is available only on the device screen. */
    nvs_handle_t ap_store;
    bool ap_key_loaded=false;
    if (nvs_open("lf_network", NVS_READWRITE, &ap_store) == ESP_OK) {
        size_t key_len=sizeof ap_pass;
        if (nvs_get_str(ap_store,"ap_key",ap_pass,&key_len)==ESP_OK &&
            strlen(ap_pass)>=8 && strlen(ap_pass)<=15) ap_key_loaded=true;
        if (!ap_key_loaded) {
            snprintf(ap_pass,sizeof ap_pass,"LF%08" PRIu32,
                     (uint32_t)(esp_random()%100000000u));
            if (nvs_set_str(ap_store,"ap_key",ap_pass)==ESP_OK)
                (void)nvs_commit(ap_store);
        }
        nvs_close(ap_store);
    } else {
        snprintf(ap_pass,sizeof ap_pass,"LF%08" PRIu32,
                 (uint32_t)(esp_random()%100000000u));
        lf_diag_error("wifi","Setup AP key could not be saved in NVS");
    }
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
