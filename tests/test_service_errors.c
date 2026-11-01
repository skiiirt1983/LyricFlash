#include "service_errors.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(lf_recognition_status(200,0,1)==LF_API_OK);
    assert(lf_recognition_status(200,0,0)==LF_API_NO_MATCH);
    assert(lf_recognition_status(200,902,0)==LF_API_QUOTA_EXHAUSTED);
    assert(lf_recognition_status(401,0,0)==LF_API_AUTH_FAILURE);
    assert(lf_recognition_status(429,0,0)==LF_API_RATE_LIMIT);
    assert(lf_recognition_status(0,0,0)==LF_API_WIFI_ERROR);
    assert(lf_recognition_status(503,0,0)==LF_API_SERVICE_UNAVAILABLE);
    assert(lf_lyrics_status(404,0)==LF_API_NO_MATCH);
    assert(lf_lyrics_status(429,0)==LF_API_RATE_LIMIT);
    puts("Recognition and lyrics service error tests passed");
}
