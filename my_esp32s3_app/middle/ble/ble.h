#ifndef __BLE_H__
#define __BLE_H__

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_WIFI_SVC_UUID 0x00FF
#define BLE_WIFI_CHR_SSID_UUID 0xFF01
#define BLE_WIFI_CHR_PASS_UUID 0xFF02

#define MAX_SSID_LEN 33
#define MAX_PASS_LEN 65

typedef void (*ble_wifi_config_cb_t)(const char *ssid, const char *password);

esp_err_t ble_wifi_init(const char *device_name);

void ble_wifi_register_callback(ble_wifi_config_cb_t cb);

esp_err_t ble_wifi_stop(void);

void ble_wifi_get_credentials(char *ssid_out, char *pass_out);

#ifdef __cplusplus
}
#endif

#endif /* __BLE_H__ */
