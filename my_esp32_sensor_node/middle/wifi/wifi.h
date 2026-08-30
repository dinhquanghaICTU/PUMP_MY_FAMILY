#ifndef __WIFI_H__
#define __WIFI_H__

#include "esp_err.h"
#include <stdint.h>

esp_err_t wifi_init_sta(uint8_t channel);

esp_err_t wifi_set_channel(uint8_t channel);

#endif // __WIFI_H__