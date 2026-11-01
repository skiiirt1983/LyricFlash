#pragma once
#include <stdint.h>

typedef enum {
    LF_API_OK, LF_API_NO_MATCH, LF_API_QUOTA_EXHAUSTED, LF_API_EXPIRED,
    LF_API_RATE_LIMIT, LF_API_AUTH_FAILURE, LF_API_WIFI_ERROR,
    LF_API_SERVICE_UNAVAILABLE, LF_API_UNKNOWN
} lf_service_error_t;
// Backend-agnostic translation of HTTP and structured provider errors.
lf_service_error_t lf_recognition_status(int http_code, int provider_code, int has_match);
lf_service_error_t lf_lyrics_status(int http_code, int has_lyrics);
const char *lf_service_error_text(lf_service_error_t error);
