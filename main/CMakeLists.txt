idf_component_register(
    SRCS "main.c" "ui.c ota_guard.c" "settings.c" "lyrics.c" "image_policy.c" "service_errors.c" "power.c" "power_logic.c" "network.c" "wifi_json.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_timer app_update esp_wifi esp_netif esp_event esp_http_server esp_partition esp_driver_i2c json
)
