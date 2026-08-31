#ifndef __LED_STATUS_H__
#define __LED_STATUS_H__

#include "esp_err.h"
#include <stdbool.h>

esp_err_t led_init(int pin);

void led_on(void);

void led_off(void);

void led_toggle(void);

void led_blink(int count, int delay_ms);

#endif // __LED_STATUS_H__
