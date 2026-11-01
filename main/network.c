#include "network.h"
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

static const char *TAG="LFNetwork";
static EventGroupHandle_t connected_event;
#define LF_CONNECTED BIT0
static bool in_ap;
static char address[32] = "Offline";
static char ap_ssid[33];
static char ap_pass[16];
static char ota_pin[10];
static TickType_t ota_expiry;
static httpd_handle_t server;
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
    xTaskCreate(reboot_task, "lf_wifi_reboot", 2048, NULL, 3, NULL);
    return true;
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
    snprintf(out, size, "%s", (const char *)ap.ssid);
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
    return lf_network_ota_armed() && strcmp(pin,ota_pin)==0;
}
static void reboot_task(void *arg)
{
    (void)arg; vTaskDelay(pdMS_TO_TICKS(900)); esp_restart();
}
static const char home_page[] =
"<!doctype html><html lang='en'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>LyricFlash Device</title><style>body{font:16px system-ui;background:#0d1518;color:#edf5f7;max-width:500px;margin:35px auto;padding:20px}"
"button,input{font:inherit;padding:12px;border-radius:12px;box-sizing:border-box;width:100%;margin:6px 0}"
"button{background:#86e3ae;border:0;color:#071b12;font-weight:700}input{background:#1b272c;color:white;border:1px solid #4c626a}"
"section{border:1px solid #34464b;border-radius:20px;padding:20px;margin:16px 0}</style>"
"<h1>LyricFlash</h1><p>ESP32-S3 / Local device manager</p>"
"<section><h2>Wi-Fi setup</h2><input id='ssid' placeholder='2.4GHz Wi-Fi name'>"
"<input id='pass' type='password' placeholder='Wi-Fi password'>"
"<button onclick='save()'>Save Wi-Fi &amp; restart</button></section>"
"<section><h2>Firmware update</h2><p>Tap <b>ARM OTA</b> on the device, then enter the PIN shown on screen.</p>"
"<input id='pin' type='password' placeholder='Device OTA PIN' autocomplete='off'>"
"<input id='bin' type='file' accept='.bin,application/octet-stream'>"
"<button onclick='update()'>Install application-only OTA firmware</button></section>"
"<p id='msg' role='status'></p><script>const msg=t=>document.getElementById('msg').textContent=t;"
"async function save(){let r=await fetch('/wifi',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({ssid:document.getElementById('ssid').value,password:document.getElementById('pass').value})});msg(await r.text());}"
"async function update(){let f=document.getElementById('bin').files[0];if(!f)return msg('Choose an OTA .bin file first');"
"msg('Uploading...');let r=await fetch('/update',{method:'POST',headers:{'Content-Type':'application/octet-stream',"
"'X-LyricFlash-PIN':document.getElementById('pin').value},body:f});msg(await r.text());}</script></html>";

static esp_err_t handle_home(httpd_req_t *req)
{
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    return httpd_resp_send(req,home_page,HTTPD_RESP_USE_STRLEN);
}
static esp_err_t handle_info(httpd_req_t *req)
{
    char out[220];
    snprintf(out,sizeof(out),"{\"app\":\"LyricFlash\",\"version\":\"0.4.0-alpha\",\"mode\":\"%s\",\"address\":\"%s\",\"ota_armed\":%s}",
        in_ap?"ap":"station",address,lf_network_ota_armed()?"true":"false");
    httpd_resp_set_type(req,"application/json");
    return httpd_resp_sendstr(req,out);
}
static esp_err_t handle_device(httpd_req_t *req)
{
    uint8_t mac[6]={0};
    esp_wifi_get_mac(in_ap?WIFI_IF_AP:WIFI_IF_STA,mac);
    char response[220];
    snprintf(response,sizeof(response),
        "{\"app\":\"LyricFlash\",\"name\":\"LyricFlash 1.75\",\"version\":\"0.4.0-alpha\","
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
    xTaskCreate(reboot_task,"lf_reboot",2048,NULL,3,NULL);
    return ESP_OK;
}
static esp_err_t handle_update(httpd_req_t *req)
{
    if(!pin_matches(req)){httpd_resp_send_err(req,HTTPD_403_FORBIDDEN,"Arm OTA on device and enter the correct PIN");return ESP_FAIL;}
    const esp_partition_t *part=esp_ota_get_next_update_partition(NULL);
    if(!part || req->content_len<512 || req->content_len>(int)part->size){
        httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"OTA image invalid or too large");return ESP_FAIL;
    }
    uint8_t buf[2048];
    uint8_t first[0x120];
    size_t first_len=0;
    int received=0;
    esp_ota_handle_t handle=0;
    bool started=false;
    bool ok=true;
    while(received<req->content_len) {
        int remaining=req->content_len-received;
        int r=httpd_req_recv(req,(char *)buf,remaining>(int)sizeof(buf)?sizeof(buf):remaining);
        if(r<=0){ok=false;break;}
        if(!started){
            size_t copy=(size_t)r;
            if(copy>sizeof(first)-first_len) copy=sizeof(first)-first_len;
            memcpy(first+first_len,buf,copy);
            first_len+=copy;
            if(first_len>=sizeof(first)){
                if(lf_check_ota_image(first,sizeof(first),part->size)!=LF_IMAGE_OK){ok=false;break;}
                if(esp_ota_begin(part,OTA_SIZE_UNKNOWN,&handle)!=ESP_OK){ok=false;break;}
                started=true;
                if(esp_ota_write(handle,first,first_len)!=ESP_OK){ok=false;break;}
                if((size_t)r>copy && esp_ota_write(handle,buf+copy,r-copy)!=ESP_OK){ok=false;break;}
            }
        } else if(esp_ota_write(handle,buf,r)!=ESP_OK){ok=false;break;}
        received+=r;
    }
    ota_expiry=0; // disarm after a single attempt, whether successful or not
    if(!ok){if(started) esp_ota_abort(handle);httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Upload failed, device remains on current firmware");return ESP_FAIL;}
    if(esp_ota_end(handle)!=ESP_OK){httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Firmware checksum / image validation failed");return ESP_FAIL;}
    if(esp_ota_set_boot_partition(part)!=ESP_OK){httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Cannot activate new firmware");return ESP_FAIL;}
    httpd_resp_sendstr(req,"Firmware verified. Rebooting into new application.");
    xTaskCreate(reboot_task,"lf_reboot",2048,NULL,3,NULL);
    return ESP_OK;
}
static bool serve(void)
{
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();
    cfg.server_port=80; cfg.stack_size=8192;
    if(httpd_start(&server,&cfg)!=ESP_OK) return false;
    const httpd_uri_t routes[]={
        {.uri="/",.method=HTTP_GET,.handler=handle_home},
        {.uri="/api/info",.method=HTTP_GET,.handler=handle_info},
        {.uri="/api/device",.method=HTTP_GET,.handler=handle_device},
        {.uri="/wifi",.method=HTTP_POST,.handler=handle_wifi},
        {.uri="/update",.method=HTTP_POST,.handler=handle_update}
    };
    for(size_t i=0;i<sizeof(routes)/sizeof(routes[0]);i++)
        if(httpd_register_uri_handler(server,&routes[i])!=ESP_OK) return false;
    return true;
}
const char *lf_network_address(void){return address;}
const char *lf_network_ap_ssid(void){return ap_ssid;}
const char *lf_network_ap_password(void){return ap_pass;}
const char *lf_network_ota_pin(void){return ota_pin;}
void lf_network_arm_ota(void){ota_expiry=xTaskGetTickCount()+pdMS_TO_TICKS(120000);}
bool lf_network_ota_armed(void){return ota_expiry && (int32_t)(ota_expiry-xTaskGetTickCount())>0;}

bool lf_network_start(void)
{
    uint32_t seed=esp_random();
    snprintf(ota_pin,sizeof ota_pin,"%08" PRIu32,seed%100000000u);
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
        in_ap=true;
        esp_wifi_stop();
    }
    /* Fallback to a password-protected access point if home Wi-Fi is unavailable. */
    in_ap=true;
    uint8_t mac[6]={0};
    esp_wifi_get_mac(WIFI_IF_STA,mac);
    snprintf(ap_ssid,sizeof ap_ssid,"LyricFlash-%02X%02X",mac[4],mac[5]);
    snprintf(ap_pass,sizeof ap_pass,"LF%06" PRIu32,(uint32_t)(esp_random()%1000000u));
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
