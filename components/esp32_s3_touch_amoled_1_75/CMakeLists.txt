idf_component_register(
    SRCS "esp32_s3_touch_amoled_1_75.c" ${SRC_VER}
    INCLUDE_DIRS "include" "include/bsp"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES driver esp_driver_i2c esp_driver_gpio esp_driver_i2s esp_driver_sdmmc esp_lcd
    PRIV_REQUIRES esp_timer spiffs esp_psram fatfs usb esp_driver_ledc
)

# -O2 enables extra flow-analysis warnings; keep them visible but non-fatal.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=maybe-uninitialized -Wno-error=format-truncation -Wno-error=stringop-truncation -Wno-error=format-overflow -Wno-error=array-bounds -Wno-error=stringop-overflow)
