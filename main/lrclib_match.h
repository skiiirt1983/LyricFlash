#pragma once
#include <stdbool.h>
#include <stddef.h>

/* Pure, host-testable LRCLIB query string helpers. Fail closed on truncation. */
bool lf_lrclib_url_encode(char *dest, size_t cap, const char *src);
bool lf_lrclib_search_title(char *dest, size_t cap, const char *title);
