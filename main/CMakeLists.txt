
# The font builder runs before ESP-IDF config in the draft build workflow.
file(GLOB CLOCK_FONT_SOURCES "${CMAKE_CURRENT_LIST_DIR}/generated_clock_fonts/*.c")
idf_component_register(
    SRCS
        ${CLOCK_FONT_SOURCES}
        "clock_fonts.c"
        "main.c"
        "diagnostics.c"
        "mic_probe.c"
        "mic_pcm.c"
        "recognition.c"
        "recognition_audio.c"
        "lrclib_match.c"
        "lrclib_client.c"
        "ui.c"
        "char_wheel.c"
        "clock_face.c"
        "basic_clock.c"
        "weather.c"
        "weather_visual.c"
        "ota_guard.c"
        "ota_health_logic.c"
        "settings.c"
        "lyrics.c"
        "lyric_clock.c"
        "lyric_sync.c"
        "song_cache.c"
        "song_history.c"
        "image_policy.c"
        "brand_icon.c"
        "service_errors.c"
        "power.c"
        "power_logic.c"
        "network.c"
        "captive_dns.c"
        "captive_dns_logic.c"
        "wifi_json.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp32_s3_touch_amoled_1_75
        nvs_flash
        esp_timer
        esp_pm
        app_update
        esp_https_ota
        esp_wifi
        esp_netif
        lwip
        esp_event
        esp_http_server
        esp_http_client
        mbedtls
        esp_partition
        esp_driver_i2c
        json
)

# -O2 enables extra flow-analysis warnings; keep them visible but non-fatal.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=maybe-uninitialized -Wno-error=format-truncation -Wno-error=stringop-truncation -Wno-error=format-overflow -Wno-error=array-bounds -Wno-error=stringop-overflow)
