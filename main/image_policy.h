#pragma once
#include <stddef.h>
#include <stdint.h>

// Application-only ESP-IDF image screening: noncryptographic preflight.
// ESP-IDF's own OTA verification still has to approve the image.
// 0 = acceptable header, nonzero = specified rejection.
typedef enum {
    LF_IMAGE_OK = 0,
    LF_IMAGE_TOO_SHORT,
    LF_IMAGE_TOO_LARGE,
    LF_IMAGE_NOT_ESP,
    LF_IMAGE_WRONG_CHIP,
    LF_IMAGE_NOT_APP,
    LF_IMAGE_WRONG_PROJECT,
} lf_image_result_t;
lf_image_result_t lf_check_ota_image(const uint8_t *data, size_t size, size_t slot_bytes);
const char *lf_image_error(lf_image_result_t result);
