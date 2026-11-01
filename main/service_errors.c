#include "service_errors.h"
lf_service_error_t lf_recognition_status(int http_code, int provider_code, int has_match)
{
    // Transport failures must not be mistaken for exhausted credits.
    if (http_code <= 0) return LF_API_WIFI_ERROR;
    if (provider_code == 902) return LF_API_QUOTA_EXHAUSTED;
    if (http_code == 429) return LF_API_RATE_LIMIT;
    if (http_code == 401 || http_code == 403) return LF_API_AUTH_FAILURE;
    if (http_code >= 500) return LF_API_SERVICE_UNAVAILABLE;
    if (http_code >= 400) return LF_API_UNKNOWN;
    return has_match ? LF_API_OK : LF_API_NO_MATCH;
}
lf_service_error_t lf_lyrics_status(int http_code, int has_lyrics)
{
    if (http_code <= 0) return LF_API_WIFI_ERROR;
    if (http_code == 429) return LF_API_RATE_LIMIT;
    if (http_code == 401 || http_code == 403) return LF_API_AUTH_FAILURE;
    if (http_code == 404) return LF_API_NO_MATCH;
    if (http_code >= 500) return LF_API_SERVICE_UNAVAILABLE;
    if (http_code >= 400) return LF_API_UNKNOWN;
    return has_lyrics ? LF_API_OK : LF_API_NO_MATCH;
}
const char *lf_service_error_text(lf_service_error_t error)
{
    switch (error) {
    case LF_API_OK: return "OK";
    case LF_API_NO_MATCH: return "Song / lyrics not found";
    case LF_API_QUOTA_EXHAUSTED: return "Out of recognition credits";
    case LF_API_EXPIRED: return "Recognition trial expired";
    case LF_API_RATE_LIMIT: return "Service busy; try again later";
    case LF_API_AUTH_FAILURE: return "API key rejected";
    case LF_API_WIFI_ERROR: return "No connection to service";
    case LF_API_SERVICE_UNAVAILABLE: return "Service temporarily unavailable";
    default: return "Recognition service error";
    }
}
