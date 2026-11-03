#include "lrclib_match.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
 char text[384], small[5], title[128];
 assert(lf_lrclib_url_encode(text,sizeof text,"A Holly Jolly Christmas (Single Version)"));
 assert(strcmp(text,"A%20Holly%20Jolly%20Christmas%20%28Single%20Version%29")==0);
 assert(lf_lrclib_url_encode(text,sizeof text,"Burl Ives"));
 assert(strcmp(text,"Burl%20Ives")==0);
 assert(lf_lrclib_url_encode(text,sizeof text,"Björk & Co."));
 assert(strcmp(text,"Bj%C3%B6rk%20%26%20Co.")==0);
 assert(!lf_lrclib_url_encode(small,sizeof small,"A Holly Jolly Christmas"));
 assert(small[0]==0);
 assert(!lf_lrclib_url_encode(text,sizeof text,""));
 assert(!lf_lrclib_url_encode(NULL,0,"title"));
 assert(lf_lrclib_search_title(title,sizeof title,"A Holly Jolly Christmas (Single Version)"));
 assert(strcmp(title,"A Holly Jolly Christmas")==0);
 assert(lf_lrclib_search_title(title,sizeof title,"Track (2011 Remaster)"));
 assert(strcmp(title,"Track")==0);
 assert(lf_lrclib_search_title(title,sizeof title,"Track (Radio Edit)"));
 assert(strcmp(title,"Track")==0);
 assert(!lf_lrclib_search_title(title,sizeof title,"Track (Live)"));
 assert(strcmp(title,"Track (Live)")==0);
 assert(!lf_lrclib_search_title(title,sizeof title,"Track Without Version"));
 assert(!lf_lrclib_search_title(title,4,"A Holly Jolly Christmas (Single Version)"));
 puts("PASS: LRCLIB percent-encoded UTF-8 metadata, version suffix fallback, no truncated query");
 return 0;
}
