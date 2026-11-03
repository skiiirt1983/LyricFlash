#include "lrclib_match.h"
#include <string.h>
#include <ctype.h>

/* Percent-encode UTF-8 bytewise. The server decodes Unicode UTF-8.
 * Never write truncated query strings: truncated titles can hit wrong songs. */
bool lf_lrclib_url_encode(char *dest, size_t cap, const char *src)
{
    if(!dest||!cap||!src)return false;
    size_t n=0;
    static const char hex[]="0123456789ABCDEF";
    for(const unsigned char *p=(const unsigned char *)src;*p;p++){
        const unsigned char c=*p;
        const bool safe=(c>='A'&&c<='Z')||(c>='a'&&c<='z')||
                        (c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~';
        const size_t extra=safe?1U:3U;
        if(extra>cap-1U-n){dest[0]=0;return false;}
        if(safe)dest[n++]=(char)c;
        else {dest[n++]='%';dest[n++]=hex[c>>4];dest[n++]=hex[c&15];}
    }
    dest[n]=0;
    return n!=0;
}
static bool contains_ci(const char *text, size_t len, const char *word)
{
    const size_t count=strlen(word);
    if(count>len)return false;
    for(size_t i=0;i+count<=len;i++){
        size_t j=0;
        for(;j<count;j++)
            if(tolower((unsigned char)text[i+j])!=tolower((unsigned char)word[j]))
                break;
        if(j==count)return true;
    }
    return false;
}
/* On exact-match 404, try a version-neutral track title once.
 * Preserve e.g. '(Live)' to avoid silently matching the wrong recording. */
bool lf_lrclib_search_title(char *dest, size_t cap, const char *title)
{
    if(!dest||cap<2||!title)return false;
    const size_t n=strlen(title);
    if(n>=cap)return false;
    memcpy(dest,title,n+1);
    size_t end=n;
    while(end>0 && dest[end-1]==' ')end--;
    dest[end]=0;
    if(end<5 || dest[end-1]!=')')return false;
    size_t begin=end-1;
    while(begin>0 && dest[begin-1]!='(')begin--;
    if(begin==0)return false;
    const char *tag=dest+begin;
    const size_t len=end-1-begin;
    if(!(contains_ci(tag,len,"version")||
         contains_ci(tag,len,"remaster")||
         contains_ci(tag,len,"radio edit")||
         contains_ci(tag,len,"single edit")))return false;
    size_t trim=begin-1;
    while(trim>0 && dest[trim-1]==' ')trim--;
    if(!trim)return false;
    dest[trim]=0;
    return true;
}
