#include "network.h"
#include "cJSON.h"
#include <stdbool.h>

bool lf_wifi_parse_and_save(const char *body)
{
    cJSON *obj = cJSON_Parse(body);
    if (!obj) return false;
    const cJSON *s = cJSON_GetObjectItemCaseSensitive(obj, "ssid");
    const cJSON *p = cJSON_GetObjectItemCaseSensitive(obj, "password");
    bool ok = cJSON_IsString(s) && cJSON_IsString(p) &&
              s->valuestring && p->valuestring &&
              lf_network_save_wifi(s->valuestring, p->valuestring);
    cJSON_Delete(obj);
    return ok;
}
