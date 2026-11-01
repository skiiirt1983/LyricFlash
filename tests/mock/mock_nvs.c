#include "nvs.h"
#include <string.h>
static struct {char key[16]; int64_t val;} vals[48];
static size_t count;
static int have_ns;
void mock_nvs_reset(void){memset(vals,0,sizeof vals);count=0;have_ns=0;}
int mock_nvs_count(void){return (int)count;}
esp_err_t nvs_open(const char *ns,int mode,nvs_handle_t *h){
 if(strcmp(ns,"lyric_ui"))return ESP_FAIL;
 if(mode==NVS_READONLY&&!have_ns)return ESP_ERR_NVS_NOT_FOUND;
 have_ns=1;*h=1;return ESP_OK;
}
void nvs_close(nvs_handle_t h){(void)h;}
static int keyidx(const char *k){for(size_t i=0;i<count;i++)if(!strcmp(vals[i].key,k))return (int)i;return -1;}
static esp_err_t get(const char *k,int64_t *value){int i=keyidx(k);if(i<0)return ESP_ERR_NVS_NOT_FOUND;*value=vals[i].val;return ESP_OK;}
static esp_err_t set(const char *k,int64_t value){int i=keyidx(k);if(i<0){if(count>=48)return ESP_FAIL;i=(int)count++;strncpy(vals[i].key,k,sizeof vals[i].key-1);}vals[i].val=value;return ESP_OK;}
esp_err_t nvs_get_u8(nvs_handle_t h,const char *k,uint8_t *v){(void)h;int64_t n;esp_err_t e=get(k,&n);if(e==ESP_OK)*v=(uint8_t)n;return e;}
esp_err_t nvs_get_u32(nvs_handle_t h,const char *k,uint32_t *v){(void)h;int64_t n;esp_err_t e=get(k,&n);if(e==ESP_OK)*v=(uint32_t)n;return e;}
esp_err_t nvs_get_i32(nvs_handle_t h,const char *k,int32_t *v){(void)h;int64_t n;esp_err_t e=get(k,&n);if(e==ESP_OK)*v=(int32_t)n;return e;}
esp_err_t nvs_set_u8(nvs_handle_t h,const char *k,uint8_t v){(void)h;return set(k,v);}
esp_err_t nvs_set_u32(nvs_handle_t h,const char *k,uint32_t v){(void)h;return set(k,v);}
esp_err_t nvs_set_i32(nvs_handle_t h,const char *k,int32_t v){(void)h;return set(k,v);}
esp_err_t nvs_commit(nvs_handle_t h){(void)h;return ESP_OK;}
