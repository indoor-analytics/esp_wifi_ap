#include "driver/rmt_tx.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_drv.h"
#include "nvs_flash.h"
#include <string.h>

static const char* TAG = "WIFI_AP";

#define WIFI_AP_SSID "ESP32_AP" WIFI_AP_SSID_VALUE
#define WIFI_AP_PASS "zacezcaazdzs"
#define WIFI_AP_CHANNEL 6
#define WIFI_AP_MAX_CONN 4

void wifi_init_softap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap =
            {
                .ssid            = WIFI_AP_SSID,
                .ssid_len        = strlen(WIFI_AP_SSID),
                .password        = WIFI_AP_PASS,
                .channel         = WIFI_AP_CHANNEL,
                .max_connection  = WIFI_AP_MAX_CONN,
                .authmode        = WIFI_AUTH_WPA2_PSK,
                .beacon_interval = 100,
            },
    };

    if (strlen(WIFI_AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP started");
    ESP_LOGI(TAG, "SSID: %s", WIFI_AP_SSID);
    ESP_LOGI(TAG, "Channel: %d", WIFI_AP_CHANNEL);
}

void app_main(void) {
    led_color_t c1 = {50, 10, 20};
    led_color_t c2 = {0, 0, 0};
    led_start_blink(c1, c2, 48, 1000);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    wifi_init_softap();

    // led_stop_blink(led_hd);
}
