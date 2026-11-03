#include "song_cache.h"
#include "nvs.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
/* Current 0x100000 NVS partition survives OTA and normal resets.
   Keep this cache bounded, never auto-delete on capacity errors, never
   format NVS. Typical timed LRC songs compact to 4-15 KB. */
#define LF_CACHE_NS "lf_songs"
#define LF_CACHE_MAGIC 0x4c465331U
#define LF_CACHE_LIMIT 19000U
static const char *TAG="SongCache";
static uint64_t hash_song(const char *title,const char *artist){
    uint64_t h=UINT64_C(14695981039346656037);
    const char *p[2]={title,artist};
    for(int part=0;part<2;part++){
        bool space=false;
        if(!p[part])return 0;
        for(const unsigned char *s=(const unsigned char *)p[part];*s;s++){
            if(isspace(*s)){space=true;continue;}
            if(space && h!=0){h^=(unsigned char)' ';h*=UINT64_C(1099511628211);}
            space=false;
            h^=(unsigned char)tolower(*s);h*=UINT64_C(1099511628211);
        }
        h^=0xFFU;h*=UINT64_C(1099511628211);
    }
    return h;
}
/* NVS keys max 15 chars. A collision is checked by stored full 64-bit
   identity and full title/artist before loading; collisions never overwrite. */
static void key(char out[16],char type,uint64_t h){
    snprintf(out,16,"%c%012llx",type,(unsigned long long)(h&UINT64_C(0xffffffffffff)));
}
static bool same_text(const char *a,const char *b){
    if(!a||!b)return false;
    while(*a && *b){
        while(isspace((unsigned char)*a))a++;
        while(isspace((unsigned char)*b))b++;
        if(!*a||!*b)break;
        if(tolower((unsigned char)*a)!=tolower((unsigned char)*b))return false;
        a++;b++;
    }
    while(isspace((unsigned char)*a))a++;
    while(isspace((unsigned char)*b))b++;
    return !*a && !*b;
}
static void put16(unsigned char **p,uint16_t n){memcpy(*p,&n,2);*p+=2;}
static void put32(unsigned char **p,uint32_t n){memcpy(*p,&n,4);*p+=4;}
static void put64(unsigned char **p,uint64_t n){memcpy(*p,&n,8);*p+=8;}
static bool get16(const unsigned char **p,const unsigned char *end,uint16_t *n){
    if((size_t)(end-*p)<2)return false;
    memcpy(n,*p,2);
    *p+=2;
    return true;
}
static bool get32(const unsigned char **p,const unsigned char *end,uint32_t *n){
    if((size_t)(end-*p)<4)return false;
    memcpy(n,*p,4);
    *p+=4;
    return true;
}
static bool get64(const unsigned char **p,const unsigned char *end,uint64_t *n){
    if((size_t)(end-*p)<8)return false;
    memcpy(n,*p,8);
    *p+=8;
    return true;
}
static bool valid_name(const char *s){return s && s[0] && strlen(s)<128;}
bool lf_song_cache_load_lyrics(const char *title,const char *artist,lf_lyrics_t *out){
    if(!out || !valid_name(title)||!valid_name(artist))return false;
    char k[16];uint64_t h=hash_song(title,artist);key(k,'L',h);
    nvs_handle_t n;
    if(nvs_open(LF_CACHE_NS,NVS_READONLY,&n)!=ESP_OK)return false;
    size_t bytes=0;
    esp_err_t err=nvs_get_blob(n,k,NULL,&bytes);
    if(err!=ESP_OK||bytes<24||bytes>LF_CACHE_LIMIT){nvs_close(n);return false;}
    unsigned char *data=heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!data){nvs_close(n);return false;}
    err=nvs_get_blob(n,k,data,&bytes);nvs_close(n);
    const unsigned char *p=data,*end=data+bytes;
    uint32_t magic=0;uint64_t full=0;uint16_t tl=0,al=0,count=0;
    bool ok=err==ESP_OK&&get32(&p,end,&magic)&&magic==LF_CACHE_MAGIC&&
        get64(&p,end,&full)&&full==h&&
        get16(&p,end,&tl)&&get16(&p,end,&al)&&get16(&p,end,&count)&&
        tl>0&&al>0&&tl<128&&al<128&&count>0&&count<=LF_MAX_LINES&&
        (size_t)(end-p)>=(size_t)tl+al;
    if(ok){
        const char *stored_title=(const char *)p,*stored_artist=(const char *)p+tl;
        ok=stored_title[tl-1]==0&&stored_artist[al-1]==0&&
           same_text(stored_title,title)&&same_text(stored_artist,artist);
        p+=tl+al;
    }
    if(ok){
        out->count=0;
        for(unsigned i=0;i<count;i++){
            uint32_t ms=0;uint16_t len=0;
            if(!get32(&p,end,&ms)||!get16(&p,end,&len)||len==0||
               len>LF_MAX_LINE_BYTES||(size_t)(end-p)<len||p[len-1]!=0){
                ok=false;break;
            }
            out->line[i].start_ms=ms;
            memcpy(out->line[i].text,p,len);p+=len;out->count++;
        }
        if(p!=end || out->count!=count)ok=false;
    }
    if(!ok)out->count=0;
    heap_caps_free(data);
    return ok;
}
bool lf_song_cache_store_lyrics(const char *title,const char *artist,const lf_lyrics_t *lines){
    if(!lines||!lines->count||lines->count>LF_MAX_LINES||
       !valid_name(title)||!valid_name(artist))return false;
    const uint64_t h=hash_song(title,artist);char k[16];key(k,'L',h);
    size_t cap=4+8+6+strlen(title)+1+strlen(artist)+1;
    for(size_t i=0;i<lines->count;i++){
        size_t length=strnlen(lines->line[i].text,LF_MAX_LINE_BYTES);
        if(length>=LF_MAX_LINE_BYTES)return false;
        cap+=6+length+1;
    }
    if(cap>LF_CACHE_LIMIT)return false;
    nvs_handle_t n;
    if(nvs_open(LF_CACHE_NS,NVS_READWRITE,&n)!=ESP_OK)return false;
    size_t old_size=0;
    if(nvs_get_blob(n,k,NULL,&old_size)==ESP_OK){
        /* Never overwrite pre-existing lyrics or a hash collision. */
        nvs_close(n);return true;
    }
    unsigned char *data=heap_caps_malloc(cap,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!data){nvs_close(n);return false;}
    unsigned char *p=data;
    put32(&p,LF_CACHE_MAGIC);put64(&p,h);
    uint16_t tl=(uint16_t)(strlen(title)+1),al=(uint16_t)(strlen(artist)+1);
    put16(&p,tl);put16(&p,al);put16(&p,(uint16_t)lines->count);
    memcpy(p,title,tl);p+=tl;memcpy(p,artist,al);p+=al;
    for(size_t i=0;i<lines->count;i++){
        uint16_t len=(uint16_t)(strlen(lines->line[i].text)+1);
        put32(&p,lines->line[i].start_ms);put16(&p,len);
        memcpy(p,lines->line[i].text,len);p+=len;
    }
    esp_err_t err=nvs_set_blob(n,k,data,cap);
    if(err==ESP_OK)err=nvs_commit(n);
    heap_caps_free(data);nvs_close(n);
    if(err!=ESP_OK)ESP_LOGW(TAG,"Cannot cache new song (NVS full or unavailable): %s",esp_err_to_name(err));
    return err==ESP_OK;
}
typedef struct {
    uint32_t magic;
    uint64_t identity;
    int32_t offset_ms;
    uint16_t rate_percent;
    uint16_t reserved;
} lf_cache_profile_t;
bool lf_song_cache_load_profile(const char *title,const char *artist,
                                int32_t *offset_ms,uint16_t *rate_percent){
    if(!offset_ms||!rate_percent||!valid_name(title)||!valid_name(artist))return false;
    const uint64_t h=hash_song(title,artist);char k[16];key(k,'P',h);
    nvs_handle_t n;
    if(nvs_open(LF_CACHE_NS,NVS_READONLY,&n)!=ESP_OK)return false;
    lf_cache_profile_t value={0};size_t size=sizeof value;
    esp_err_t e=nvs_get_blob(n,k,&value,&size);nvs_close(n);
    if(e!=ESP_OK||size!=sizeof value||value.magic!=LF_CACHE_MAGIC||
       value.identity!=h||value.offset_ms < -10000||value.offset_ms>10000||
       value.rate_percent<75||value.rate_percent>125)return false;
    *offset_ms=value.offset_ms;*rate_percent=value.rate_percent;
    return true;
}
bool lf_song_cache_store_profile(const char *title,const char *artist,
                                 int32_t offset_ms,uint16_t rate_percent){
    if(!valid_name(title)||!valid_name(artist)||offset_ms < -10000||
       offset_ms>10000||rate_percent<75||rate_percent>125)return false;
    int32_t old_offset=0;uint16_t old_rate=0;
    if(lf_song_cache_load_profile(title,artist,&old_offset,&old_rate)&&
       old_offset==offset_ms && old_rate==rate_percent)return true;
    const uint64_t h=hash_song(title,artist);char k[16];key(k,'P',h);
    nvs_handle_t n;
    if(nvs_open(LF_CACHE_NS,NVS_READWRITE,&n)!=ESP_OK)return false;
    const lf_cache_profile_t value={LF_CACHE_MAGIC,h,offset_ms,rate_percent,0};
    esp_err_t err=nvs_set_blob(n,k,&value,sizeof value);
    if(err==ESP_OK)err=nvs_commit(n);
    nvs_close(n);
    return err==ESP_OK;
}
