#include "image_policy.h"
#include <string.h>

// ESP-IDF 5.5 app image: 24-byte image header, then 8-byte segment header,
// then esp_app_desc_t starts at offset 0x20. esp_app_desc_t.project_name at +48.
#define LF_DESC_OFFSET 0x20u
#define LF_PROJECT_OFFSET (LF_DESC_OFFSET + 48u)
#define LF_MIN_IMAGE 0x120u
#define LF_EXPECTED_CHIP_ID 9u

lf_image_result_t lf_check_ota_image(const uint8_t *data, size_t size, size_t slot_bytes)
{
    static const uint8_t desc_magic[] = {0x32, 0x54, 0xCD, 0xAB};
    static const char project[] = "lyricflash";
    if (!data || size < LF_MIN_IMAGE) return LF_IMAGE_TOO_SHORT;
    if (size > slot_bytes || slot_bytes == 0) return LF_IMAGE_TOO_LARGE;
    if (data[0] != 0xe9) return LF_IMAGE_NOT_ESP;
    if (data[12] != LF_EXPECTED_CHIP_ID || data[13] != 0) return LF_IMAGE_WRONG_CHIP;
    if (memcmp(data + LF_DESC_OFFSET, desc_magic, sizeof desc_magic) != 0) return LF_IMAGE_NOT_APP;
    if (memcmp(data + LF_PROJECT_OFFSET, project, sizeof project) != 0)
        return LF_IMAGE_WRONG_PROJECT;
    return LF_IMAGE_OK;
}

const char *lf_image_error(lf_image_result_t result)
{
    switch (result) {
    case LF_IMAGE_OK: return "Header accepted; final ESP-IDF signature/integrity checks still required";
    case LF_IMAGE_TOO_SHORT: return "Incomplete OTA application image";
    case LF_IMAGE_TOO_LARGE: return "Application image exceeds inactive OTA slot";
    case LF_IMAGE_NOT_ESP: return "Invalid ESP firmware image";
    case LF_IMAGE_WRONG_CHIP: return "Firmware is not for ESP32-S3";
    case LF_IMAGE_NOT_APP: return "Expected an application-only OTA image, not a merged factory image";
    case LF_IMAGE_WRONG_PROJECT: return "This update is not LyricFlash firmware";
    default: return "Unrecognized firmware error";
    }
}
