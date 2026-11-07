#include "song_history.h"
#include "nvs.h"
#include "esp_heap_caps.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* Store ONLY title and artist, without audio, lyrics duplication or timestamps.
   Reading existing L keys makes the previously cached song database visible. */
#define LF_HISTORY_NS "lf_songs"
#define LF_HISTORY_MAGIC UINT32_C(0x4c464831)
#define LF_LYRICS_MAGIC UINT32_C(0x4c465331)
#define LF_MAX_LYRICS_BLOB 19000U

static uint64_t identity(const char *title,const char *artist)
{
    uint64_t h=UINT64_C(14695981039346656037);
    const char *parts[2]={title,artist};
    for(int k=0;k<2;k++){
        bool space=false;
        if(!parts[k])return 0;
        for(const unsigned char *p=(const unsigned char *)parts[k];*p;p++){
            if(isspace(*p)){space=true;continue;}
            if(space){h^=' ';h*=UINT64_C(1099511628211);space=false;}
            h^=(unsigned char)tolower(*p);h*=UINT64_C(1099511628211);
        }
        h^=255U;h*=UINT64_C(1099511628211);
    }
    return h;
}
static void name_key(char key[16],char prefix,uint64_t hash)
{
    snprintf(key,16,"%c%012llx",prefix,
             (unsigned long long)(hash&UINT64_C(0xffffffffffff)));
}
bool lf_song_history_remember(const char *title,const char *artist)
{
    if(!title||!artist||!title[0]||!artist[0]||
       strlen(title)>=128||strlen(artist)>=128)return false;
    uint64_t h=identity(title,artist);
    char hkey[16],lkey[16];name_key(hkey,'H',h);name_key(lkey,'L',h);
    nvs_handle_t n;
    if(nvs_open(LF_HISTORY_NS,NVS_READWRITE,&n)!=ESP_OK)return false;
    size_t previous=0;
    /* Already indexed, or already has original immutable lyric cache entry:
       no redundant NVS writes and no modified existing keys. */
    if(nvs_get_blob(n,hkey,NULL,&previous)==ESP_OK ||
       nvs_get_blob(n,lkey,NULL,&previous)==ESP_OK){
        nvs_close(n);return true;
    }
    const uint16_t tl=(uint16_t)(strlen(title)+1),al=(uint16_t)(strlen(artist)+1);
    uint8_t raw[16+256]={0};
    uint32_t magic=LF_HISTORY_MAGIC;
    memcpy(raw,&magic,4);memcpy(raw+4,&h,8);
    memcpy(raw+12,&tl,2);memcpy(raw+14,&al,2);
    memcpy(raw+16,title,tl);memcpy(raw+16+tl,artist,al);
    esp_err_t e=nvs_set_blob(n,hkey,raw,16U+tl+al);
    if(e==ESP_OK)e=nvs_commit(n);
    nvs_close(n);
    return e==ESP_OK;
}
static bool parse_blob(nvs_handle_t n,const char *key,lf_song_history_item_t *item,
                       uint64_t *hash)
{
    size_t len=0;
    if(nvs_get_blob(n,key,NULL,&len)!=ESP_OK||len<20||
       len>LF_MAX_LYRICS_BLOB)return false;
    uint8_t *raw=heap_caps_malloc(len,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!raw)return false;
    bool valid=nvs_get_blob(n,key,raw,&len)==ESP_OK;
    uint32_t magic=0;uint64_t h=0;uint16_t tl=0,al=0;
    if(valid){
        memcpy(&magic,raw,4);memcpy(&h,raw+4,8);
        memcpy(&tl,raw+12,2);memcpy(&al,raw+14,2);
        const size_t offset=key[0]=='L'?18U:16U;
        valid=(magic==(key[0]=='L'?LF_LYRICS_MAGIC:LF_HISTORY_MAGIC)) &&
            tl>1 && tl<=128 && al>1 && al<=128 &&
            len>=offset+(size_t)tl+al &&
            raw[offset+tl-1]==0 && raw[offset+tl+al-1]==0;
        if(valid){
            memcpy(item->title,raw+offset,tl);
            memcpy(item->artist,raw+offset+tl,al);
            valid=identity(item->title,item->artist)==h;
            if(valid)*hash=h;
        }
    }
    heap_caps_free(raw);
    return valid;
}
size_t lf_song_history_list(lf_song_history_item_t *out,size_t cap,
                            size_t offset,size_t *total)
{
    if(total)*total=0;
    if(cap && !out)return 0;
    nvs_handle_t n;
    if(nvs_open(LF_HISTORY_NS,NVS_READONLY,&n)!=ESP_OK)return 0;
    nvs_iterator_t it=NULL;
    esp_err_t err=nvs_entry_find("nvs",LF_HISTORY_NS,NVS_TYPE_BLOB,&it);
    size_t seen=0,written=0;
    while(err==ESP_OK && it){
        nvs_entry_info_t info={0};
        nvs_entry_info(it,&info);
        if((info.key[0]=='L'||info.key[0]=='H') && strlen(info.key)==13){
            lf_song_history_item_t entry={0};
            uint64_t h=0;
            if(parse_blob(n,info.key,&entry,&h)){
                char hkey[16],lkey[16];name_key(hkey,'H',h);name_key(lkey,'L',h);
                size_t value_len=0;
                /* When a track has both keys, H owns the history row and
                   the earlier immutable L cache contributes the lyrics flag. */
                if(info.key[0]=='L' && nvs_get_blob(n,hkey,NULL,&value_len)==ESP_OK)
                    goto next_entry;
                entry.lyrics_cached=info.key[0]=='L' ||
                    nvs_get_blob(n,lkey,NULL,&value_len)==ESP_OK;
                if(seen>=offset && written<cap)out[written++]=entry;
                seen++;
            }
        }
next_entry:
        err=nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
    nvs_close(n);
    if(total)*total=seen;
    return written;
}
