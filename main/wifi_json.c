#include "nvs.h"
#include "cJSON.h"
#include <stdbool.h>
#include <string.h>

bool lf_wifi_parse_and_save(const char *body)
{
    cJSON *obj=cJSON_Parse(body);
    if(!obj) return false;
    const cJSON *s=cJSON_GetObjectItemCaseSensitive(obj,"ssid");
    const cJSON *p=cJSON_GetObjectItemCaseSensitive(obj,"password");
    bool ok=false;
    if(cJSON_IsString(s) && cJSON_IsString(p) && s->valuestring && p->valuestring) {
        size_t n=strlen(s->valuestring), m=strlen(p->valuestring);
        if(n>=1 && n<=32 && (m==0 || (m>=8 && m<=63))) {
            nvs_handle_t h;
            if(nvs_open("lf_network",NVS_READWRITE,&h)==ESP_OK){
                esp_err_t a=nvs_set_str(h,"ssid",s->valuestring);
                esp_err_t b=nvs_set_str(h,"password",p->valuestring);
                ok=(a==ESP_OK && b==ESP_OK && nvs_commit(h)==ESP_OK);
                nvs_close(h);
            }
        }
    }
    cJSON_Delete(obj);
    return ok;
}
