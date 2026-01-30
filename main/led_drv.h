#ifndef _LED_DRV_H_
#define _LED_DRV_H_

#include <stdint.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} led_color_t;

void* led_start_blink(led_color_t blk_c1, led_color_t blk_c2, const int led_gpio, const int blk_period);
void  led_change_blink_color(void* led_hdl, led_color_t blk_c1, led_color_t blk_c2);
void  led_change_blink_period(void* led_hdl, const int blk_period);
void  led_stop_blink(void* led_hdl);

#endif /* _LED_DRV_H_ */
