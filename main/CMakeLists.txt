idf_component_register(SRCS "main.c" "led_drv.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_driver_rmt esp_wifi nvs_flash
)

target_compile_definitions(${COMPONENT_TARGET} PRIVATE WIFI_AP_SSID_VALUE=\"${SSID_VALUE}\")

