/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 * From https://github.com/espressif/esp-idf/tree/master/examples/peripherals/rmt/led_strip
 */

#include "led_drv.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include <stdint.h>

#define RMT_LED_RESOLUTION_HZ 10000000
#define RGB_LED_COUNT 1
static const char* TAG = "led_encoder";

/**
 * @brief Type of led strip encoder configuration
 */
typedef struct {
    uint32_t resolution; /*!< Encoder resolution, in Hz */
} led_strip_encoder_config_t;

typedef struct {
    rmt_encoder_t     base;
    rmt_encoder_t*    bytes_encoder;
    rmt_encoder_t*    copy_encoder;
    int               state;
    rmt_symbol_word_t reset_code;
} rmt_led_strip_encoder_t;

typedef struct {
    rmt_channel_handle_t rmt_chan;
    rmt_encoder_handle_t led_encoder;
    uint8_t              led_pixel[3]; // GRB
    led_color_t          blk_color1;
    led_color_t          blk_color2;
    int                  blk_state;
    int                  gpio;
    TaskHandle_t         xHandle;
    TickType_t           xDelay;
} blk_led_t;

RMT_ENCODER_FUNC_ATTR
static size_t rmt_encode_led_strip(rmt_encoder_t* encoder, rmt_channel_handle_t channel, const void* primary_data, size_t data_size, rmt_encode_state_t* ret_state) {
    rmt_led_strip_encoder_t* led_encoder     = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_handle_t     bytes_encoder   = led_encoder->bytes_encoder;
    rmt_encoder_handle_t     copy_encoder    = led_encoder->copy_encoder;
    rmt_encode_state_t       session_state   = RMT_ENCODING_RESET;
    rmt_encode_state_t       state           = RMT_ENCODING_RESET;
    size_t                   encoded_symbols = 0;
    switch (led_encoder->state) {
        case 0: // send RGB data
            encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, primary_data, data_size, &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                led_encoder->state = 1; // switch to next state when current encoding session finished
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                goto out; // yield if there's no free space for encoding artifacts
            }
        // fall-through
        case 1: // send reset code
            encoded_symbols += copy_encoder->encode(copy_encoder, channel, &led_encoder->reset_code, sizeof(led_encoder->reset_code), &session_state);
            if (session_state & RMT_ENCODING_COMPLETE) {
                led_encoder->state = RMT_ENCODING_RESET; // back to the initial encoding session
                state |= RMT_ENCODING_COMPLETE;
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                goto out; // yield if there's no free space for encoding artifacts
            }
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t* encoder) {
    rmt_led_strip_encoder_t* led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_del_encoder(led_encoder->bytes_encoder);
    rmt_del_encoder(led_encoder->copy_encoder);
    free(led_encoder);
    return ESP_OK;
}

RMT_ENCODER_FUNC_ATTR
static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t* encoder) {
    rmt_led_strip_encoder_t* led_encoder = __containerof(encoder, rmt_led_strip_encoder_t, base);
    rmt_encoder_reset(led_encoder->bytes_encoder);
    rmt_encoder_reset(led_encoder->copy_encoder);
    led_encoder->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

static esp_err_t rmt_new_led_strip_encoder(const led_strip_encoder_config_t* config, rmt_encoder_handle_t* ret_encoder) {
    esp_err_t                ret         = ESP_OK;
    rmt_led_strip_encoder_t* led_encoder = NULL;
    ESP_GOTO_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    led_encoder = rmt_alloc_encoder_mem(sizeof(rmt_led_strip_encoder_t));
    ESP_GOTO_ON_FALSE(led_encoder, ESP_ERR_NO_MEM, err, TAG, "no mem for led strip encoder");
    led_encoder->base.encode = rmt_encode_led_strip;
    led_encoder->base.del    = rmt_del_led_strip_encoder;
    led_encoder->base.reset  = rmt_led_strip_encoder_reset;
    // different led strip might have its own timing requirements, following parameter is for WS2812
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 =
            {
                .level0    = 1,
                .duration0 = 0.3 * config->resolution / 1000000, // T0H=0.3us
                .level1    = 0,
                .duration1 = 0.9 * config->resolution / 1000000, // T0L=0.9us
            },
        .bit1 =
            {
                .level0    = 1,
                .duration0 = 0.9 * config->resolution / 1000000, // T1H=0.9us
                .level1    = 0,
                .duration1 = 0.3 * config->resolution / 1000000, // T1L=0.3us
            },
        .flags.msb_first = 1 // WS2812 transfer bit order: G7...G0R7...R0B7...B0
    };
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder->bytes_encoder), err, TAG, "create bytes encoder failed");
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &led_encoder->copy_encoder), err, TAG, "create copy encoder failed");

    uint32_t reset_ticks    = config->resolution / 1000000 * 50 / 2; // reset code duration defaults to 50us
    led_encoder->reset_code = (rmt_symbol_word_t){
        .level0    = 0,
        .duration0 = reset_ticks,
        .level1    = 0,
        .duration1 = reset_ticks,
    };
    *ret_encoder = &led_encoder->base;
    return ESP_OK;
err:
    if (led_encoder) {
        if (led_encoder->bytes_encoder) {
            rmt_del_encoder(led_encoder->bytes_encoder);
        }
        if (led_encoder->copy_encoder) {
            rmt_del_encoder(led_encoder->copy_encoder);
        }
        free(led_encoder);
    }
    return ret;
}

static void rgb_led_init(blk_led_t* led) {

    rmt_tx_channel_config_t tx_cfg = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .gpio_num          = led->gpio, /* GPIO 48 on esp32-s3 devkitC-1 v1.6 */
        .mem_block_symbols = 64,
        .resolution_hz     = RMT_LED_RESOLUTION_HZ,
        .trans_queue_depth = 1,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_cfg, &led->rmt_chan));

    led_strip_encoder_config_t enc_cfg = {
        .resolution = RMT_LED_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&enc_cfg, &led->led_encoder));
    ESP_ERROR_CHECK(rmt_enable(led->rmt_chan));
}

static void rgb_led_set(const blk_led_t* led, const led_color_t blk_color) {
    uint8_t led_pixel[3];
    led_pixel[0] = blk_color.g;
    led_pixel[1] = blk_color.r;
    led_pixel[2] = blk_color.b;

    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,
    };
    ESP_ERROR_CHECK(rmt_transmit(led->rmt_chan, led->led_encoder, led_pixel, sizeof(led_pixel), &tx_cfg));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led->rmt_chan, portMAX_DELAY));
}

static void blink_task(void* arg) {
    blk_led_t* led = (blk_led_t*)arg;
    for (;;) {
        vTaskDelay(led->xDelay);
        led_color_t blk_color;
        if (led->blk_state)
            blk_color = led->blk_color1;
        else
            blk_color = led->blk_color2;
        rgb_led_set(led, blk_color);
        led->blk_state ^= 1;
    }
}

/*
 * Starts blinking the led on given gpio.
 * blk_c1 : color 1 of blink
 * blk_c2 : color 2 of blink
 * led_gpio : gpio of the led to blink
 * blk_period : duration of each blink in ms
 * returns : led handle
 */
void* led_start_blink(led_color_t blk_c1, led_color_t blk_c2, const int led_gpio, const int blk_period) {
    blk_led_t* led = malloc(sizeof(blk_led_t));
    if (led == NULL) {
        ESP_LOGE(TAG, "Could not allocate memory");
        return NULL;
    }
    led->blk_color1 = blk_c1;
    led->blk_color2 = blk_c2;
    led->blk_state  = 1;
    led->gpio       = led_gpio;
    led->xHandle    = NULL;
    led->xDelay     = blk_period / portTICK_PERIOD_MS;
    rgb_led_init(led);
    ESP_LOGI("LED", "Start blinking");
    xTaskCreate(blink_task, "LED_BLK", 1024, (void*)led, tskIDLE_PRIORITY, &led->xHandle);
    configASSERT(led->xHandle);

    return (void*)led;
}

void led_change_blink_color(void* led_hdl, led_color_t blk_c1, led_color_t blk_c2) {
    blk_led_t* led  = (blk_led_t*)led_hdl;
    led->blk_color1 = blk_c1;
    led->blk_color2 = blk_c2;
}

void led_change_blink_period(void* led_hdl, const int blk_period) {
    blk_led_t* led = (blk_led_t*)led_hdl;
    led->xDelay    = blk_period / portTICK_PERIOD_MS;
}

void led_stop_blink(void* led_hdl) {
    blk_led_t* led = (blk_led_t*)led_hdl;
    if (led->xHandle != NULL) {
        vTaskDelete(led->xHandle);
    }
    free(led);
}
