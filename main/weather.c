#include "weather.h"
#include "diagnostics.h"
#include "network.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static portMUX_TYPE weather_mux = portMUX_INITIALIZER_UNLOCKED;
static lf_weather_config_t config = {.enabled = false, .interval_minutes = 30, .city = ""};
static char weather_strip[68];
static char weather_details[256];
static int weather_temperature;
static int weather_code;
static bool weather_numeric_valid;
static lf_weather_view_t view_cache;
static uint32_t config_generation;
static TaskHandle_t weather_task;

typedef struct {
    char *data;
    size_t capacity;
    size_t length;
    bool overflow;
} response_t;

static esp_err_t on_http_event(esp_http_client_event_t *event)
{
    response_t *out = (response_t *)event->user_data;
    if (event->event_id == HTTP_EVENT_ON_DATA && out && event->data_len > 0) {
        size_t len = (size_t)event->data_len;
        if (len >= out->capacity - out->length) {
            out->overflow = true;
            return ESP_FAIL;
        }
        memcpy(out->data + out->length, event->data, len);
        out->length += len;
        out->data[out->length] = 0;
    }
    return ESP_OK;
}
static cJSON *https_json(const char *url)
{
    response_t out = {.capacity = 12288};
    out.data = calloc(out.capacity, 1);
    if (!out.data) return NULL;
    esp_http_client_config_t options = {
        .url = url,
        .event_handler = on_http_event,
        .user_data = &out,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 9000,
        .buffer_size = 1024,
        .disable_auto_redirect = false,
        .max_redirection_count = 2
    };
    esp_http_client_handle_t client = esp_http_client_init(&options);
    if (!client) { free(out.data); return NULL; }
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    cJSON *result = NULL;
    if (err == ESP_OK && !out.overflow && status == 200 && out.length > 0)
        result = cJSON_Parse(out.data);
    free(out.data);
    return result;
}
/* Percent-encode UTF-8 bytes for the URL query string. */
static bool encode_city(const char *input, char *encoded, size_t cap)
{
    size_t w = 0;
    for (const unsigned char *p = (const unsigned char *)input; *p; p++) {
        bool safe = isalnum(*p) || *p == '-' || *p == '_' || *p == '.';
        size_t n = safe ? 1 : 3;
        if (w + n >= cap) return false;
        if (safe) encoded[w++] = (char)*p;
        else {
            const char *hex = "0123456789ABCDEF";
            encoded[w++] = '%';
            encoded[w++] = hex[*p >> 4];
            encoded[w++] = hex[*p & 15];
        }
    }
    encoded[w] = 0;
    return w > 0;
}
static const char *condition(int code)
{
    if (code == 0) return "CLEAR";
    if (code <= 3) return "CLOUD";
    if (code == 45 || code == 48) return "FOG";
    if (code >= 51 && code <= 67) return "RAIN";
    if (code >= 71 && code <= 77) return "SNOW";
    if (code >= 80 && code <= 82) return "RAIN";
    if (code == 85 || code == 86) return "SNOW";
    if (code >= 95) return "STORM";
    return "WEATHER";
}
static bool fetch_weather(const char *city, char *result, size_t cap,
                          char *details, size_t details_cap, int *out_temp, int *out_code,
                          lf_weather_view_t *view)
{
    char encoded[3 * (LF_WEATHER_CITY_MAX + 1)] = {0};
    if (!encode_city(city, encoded, sizeof encoded)) return false;
    char url[512];
    snprintf(url, sizeof url,
             "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1&language=en&format=json",
             encoded);
    cJSON *geo = https_json(url);
    if (!geo) return false;
    cJSON *matches = cJSON_GetObjectItemCaseSensitive(geo, "results");
    cJSON *first = cJSON_IsArray(matches) ? cJSON_GetArrayItem(matches, 0) : NULL;
    cJSON *latitude = cJSON_GetObjectItemCaseSensitive(first, "latitude");
    cJSON *longitude = cJSON_GetObjectItemCaseSensitive(first, "longitude");
    cJSON *resolved_name = cJSON_GetObjectItemCaseSensitive(first, "name");
    bool valid = cJSON_IsNumber(latitude) && cJSON_IsNumber(longitude);
    double lat = valid ? latitude->valuedouble : 0;
    double lon = valid ? longitude->valuedouble : 0;
    char short_name[19] = {0};
    if (cJSON_IsString(resolved_name) && resolved_name->valuestring)
        snprintf(short_name, sizeof short_name, "%.18s", resolved_name->valuestring);
    else snprintf(short_name, sizeof short_name, "%.18s", city);
    cJSON_Delete(geo);
    if (!valid) return false;

    snprintf(url, sizeof url,
             "https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f"
             "&current=temperature_2m,apparent_temperature,relative_humidity_2m,"
             "weather_code,wind_speed_10m,precipitation"
             "&hourly=temperature_2m,weather_code&forecast_hours=8"
             "&daily=temperature_2m_max,temperature_2m_min,weather_code&forecast_days=3"
             "&temperature_unit=celsius&timezone=auto",
             lat, lon);
    cJSON *forecast = https_json(url);
    if (!forecast) return false;
    cJSON *current = cJSON_GetObjectItemCaseSensitive(forecast, "current");
    cJSON *temp = cJSON_GetObjectItemCaseSensitive(current, "temperature_2m");
    cJSON *code = cJSON_GetObjectItemCaseSensitive(current, "weather_code");
    valid = cJSON_IsNumber(temp) && cJSON_IsNumber(code);
    if (valid) {
        double t = temp->valuedouble;
        int rounded = (int)(t < 0 ? t - 0.5 : t + 0.5);
        snprintf(result, cap, "%s  %dC  %s", short_name, rounded, condition(code->valueint));
        if (out_temp) *out_temp = rounded;
        if (out_code) *out_code = code->valueint;
    }
    if (valid && view) {
        memset(view, 0, sizeof *view);
        view->valid = true;
        snprintf(view->location, sizeof view->location, "%s", short_name);
        view->temperature = (int)(temp->valuedouble + (temp->valuedouble >= 0 ? 0.5 : -0.5));
        view->condition = code->valueint;
    }
    if (valid && details && details_cap) {
        cJSON *feels = cJSON_GetObjectItemCaseSensitive(current, "apparent_temperature");
        cJSON *humidity = cJSON_GetObjectItemCaseSensitive(current, "relative_humidity_2m");
        cJSON *wind = cJSON_GetObjectItemCaseSensitive(current, "wind_speed_10m");
        cJSON *precip = cJSON_GetObjectItemCaseSensitive(current, "precipitation");
        double perceived = cJSON_IsNumber(feels) ? feels->valuedouble : temp->valuedouble;
        double wind_kmh = cJSON_IsNumber(wind) ? wind->valuedouble : 0;
        double rain_mm = cJSON_IsNumber(precip) ? precip->valuedouble : 0;
        int humidity_pct = cJSON_IsNumber(humidity) ? humidity->valueint : 0;
        if (view) {
            view->feels_like = (int)(perceived + (perceived >= 0 ? 0.5 : -0.5));
            view->humidity_pct = humidity_pct;
            view->wind_kmh = (int)(wind_kmh + 0.5);
            view->precipitation_tenths_mm = (int)(rain_mm * 10.0 + 0.5);
        }
        snprintf(details, details_cap,
                 "%s\n%d C  %s\nFeels like %.0f C\nHumidity %d%%\nWind %.0f km/h\nPrecipitation %.1f mm",
                 short_name, (int)(temp->valuedouble < 0 ? temp->valuedouble-0.5 : temp->valuedouble+0.5),
                 condition(code->valueint), perceived, humidity_pct, wind_kmh, rain_mm);
    }
    if (valid && view) {
        cJSON *hourly = cJSON_GetObjectItemCaseSensitive(forecast, "hourly");
        cJSON *hours = cJSON_GetObjectItemCaseSensitive(hourly, "time");
        cJSON *temps = cJSON_GetObjectItemCaseSensitive(hourly, "temperature_2m");
        cJSON *codes = cJSON_GetObjectItemCaseSensitive(hourly, "weather_code");
        for (unsigned i=0;i<LF_WEATHER_HOURS;i++) {
            /* Four samples spanning the next 8 hours. */
            unsigned index = i*2;
            cJSON *at = cJSON_GetArrayItem(hours, (int)index);
            cJSON *t = cJSON_GetArrayItem(temps, (int)index);
            cJSON *c = cJSON_GetArrayItem(codes, (int)index);
            if (!cJSON_IsString(at) || !cJSON_IsNumber(t) || !cJSON_IsNumber(c)) break;
            const char *clock = strchr(at->valuestring, 'T');
            snprintf(view->hourly[i].hour, sizeof view->hourly[i].hour,
                     "%.2sh", clock ? clock+1 : "--");
            view->hourly[i].temperature = (int)(t->valuedouble + (t->valuedouble >= 0 ? 0.5 : -0.5));
            view->hourly[i].condition = c->valueint;
            view->hour_count++;
        }
        cJSON *daily = cJSON_GetObjectItemCaseSensitive(forecast, "daily");
        cJSON *highs = cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_max");
        cJSON *lows = cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_min");
        cJSON *day_codes = cJSON_GetObjectItemCaseSensitive(daily, "weather_code");
        for (unsigned i=0;i<LF_WEATHER_DAYS;i++) {
            cJSON *hi = cJSON_GetArrayItem(highs, (int)i);
            cJSON *lo = cJSON_GetArrayItem(lows, (int)i);
            cJSON *c = cJSON_GetArrayItem(day_codes, (int)i);
            if (!cJSON_IsNumber(hi) || !cJSON_IsNumber(lo) || !cJSON_IsNumber(c)) break;
            view->daily[i].high = (int)(hi->valuedouble + (hi->valuedouble >= 0 ? 0.5 : -0.5));
            view->daily[i].low = (int)(lo->valuedouble + (lo->valuedouble >= 0 ? 0.5 : -0.5));
            view->daily[i].condition = c->valueint;
            view->day_count++;
        }
    }
    cJSON_Delete(forecast);
    return valid;
}
static void set_strip(const char *message)
{
    portENTER_CRITICAL(&weather_mux);
    snprintf(weather_strip, sizeof(weather_strip), "%s", message ? message : "");
    portEXIT_CRITICAL(&weather_mux);
}
static bool valid_interval(uint16_t value)
{
    return value == 15 || value == 30 || value == 60 || value == 120;
}
static void load_config(void)
{
    nvs_handle_t h;
    if (nvs_open("lf_weather", NVS_READONLY, &h) != ESP_OK) return;
    lf_weather_config_t loaded = {.enabled = false, .interval_minutes = 30, .city = ""};
    uint8_t en = 0;
    (void)nvs_get_u8(h, "enabled", &en);
    loaded.enabled = en == 1;
    (void)nvs_get_u16(h, "interval", &loaded.interval_minutes);
    size_t cap = sizeof loaded.city;
    (void)nvs_get_str(h, "city", loaded.city, &cap);
    nvs_close(h);
    if (!valid_interval(loaded.interval_minutes)) loaded.interval_minutes = 30;
    portENTER_CRITICAL(&weather_mux);
    config = loaded;
    config_generation++;
    portEXIT_CRITICAL(&weather_mux);
}
void lf_weather_get_config(lf_weather_config_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&weather_mux);
    *out = config;
    portEXIT_CRITICAL(&weather_mux);
}
bool lf_weather_save_config(const lf_weather_config_t *next)
{
    if (!next || !valid_interval(next->interval_minutes)) return false;
    size_t length = strnlen(next->city, LF_WEATHER_CITY_MAX + 1);
    if (length > LF_WEATHER_CITY_MAX) return false;
    nvs_handle_t h;
    if (nvs_open("lf_weather", NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t e = nvs_set_u8(h, "enabled", next->enabled ? 1 : 0);
    if (e == ESP_OK) e = nvs_set_u16(h, "interval", next->interval_minutes);
    if (e == ESP_OK) e = nvs_set_str(h, "city", next->city);
    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);
    if (e != ESP_OK) return false;
    portENTER_CRITICAL(&weather_mux);
    config = *next;
    config_generation++;
    portEXIT_CRITICAL(&weather_mux);
    set_strip("");
    portENTER_CRITICAL(&weather_mux);
    weather_details[0] = 0;
    weather_numeric_valid = false;
    view_cache.valid = false;
    portEXIT_CRITICAL(&weather_mux);
    return true;
}
bool lf_weather_get_current(int *temp_c, int *wmo_code)
{
    portENTER_CRITICAL(&weather_mux);
    bool valid = weather_numeric_valid;
    if (valid) {
        if (temp_c) *temp_c = weather_temperature;
        if (wmo_code) *wmo_code = weather_code;
    }
    portEXIT_CRITICAL(&weather_mux);
    return valid;
}
void lf_weather_get_view(lf_weather_view_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&weather_mux);
    *out = view_cache;
    portEXIT_CRITICAL(&weather_mux);
}
void lf_weather_get_detail(char *out, size_t max)
{
    if (!out || !max) return;
    portENTER_CRITICAL(&weather_mux);
    snprintf(out, max, "%s", weather_details[0] ? weather_details : "Weather not available yet");
    portEXIT_CRITICAL(&weather_mux);
}
void lf_weather_get_strip(char *out, size_t max)
{
    if (!out || !max) return;
    portENTER_CRITICAL(&weather_mux);
    snprintf(out, max, "%s", weather_strip);
    portEXIT_CRITICAL(&weather_mux);
}
static void worker(void *arg)
{
    (void)arg;
    uint32_t prior_generation = 0;
    TickType_t next_poll = 0;
    for (;;) {
        lf_weather_config_t c;
        uint32_t generation;
        portENTER_CRITICAL(&weather_mux);
        c = config;
        generation = config_generation;
        portEXIT_CRITICAL(&weather_mux);

        if (!c.enabled) {
            set_strip("");
        } else if (!c.city[0]) {
            set_strip("SET WEATHER CITY");
        } else if (!lf_network_is_connected()) {
            set_strip("WEATHER OFFLINE");
        } else if (generation != prior_generation ||
                   (int32_t)(xTaskGetTickCount() - next_poll) >= 0) {
            char reading[68] = {0};
            char detailed[256] = {0};
            int t=0, code=0;
            lf_weather_view_t updated_view = {0};
            bool ok = fetch_weather(c.city, reading, sizeof reading,
                                    detailed, sizeof detailed, &t, &code, &updated_view);
            /* A changed configuration must not display a stale result. */
            portENTER_CRITICAL(&weather_mux);
            bool unchanged = config_generation == generation;
            portEXIT_CRITICAL(&weather_mux);
            if (unchanged) {
                set_strip(ok ? reading : "WEATHER UNAVAILABLE");
                if (!ok) lf_diag_error("weather", "forecast request failed");
                portENTER_CRITICAL(&weather_mux);
                snprintf(weather_details, sizeof weather_details, "%s",
                         ok ? detailed : "Weather unavailable. Check Wi-Fi and the selected city.");
                if (ok) {
                    weather_temperature = t;
                    weather_code = code;
                    weather_numeric_valid = true;
                    view_cache = updated_view;
                }
                portEXIT_CRITICAL(&weather_mux);
            }
            uint32_t delay_ms = ok ? ((uint32_t)c.interval_minutes * 60000U) : 60000U;
            next_poll = xTaskGetTickCount() + pdMS_TO_TICKS(delay_ms);
        }
        prior_generation = generation;
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
bool lf_weather_start(void)
{
    if (weather_task) return true;
    load_config();
    return xTaskCreate(worker, "lf_weather", 8192, NULL,
                       tskIDLE_PRIORITY + 1, &weather_task) == pdPASS;
}
