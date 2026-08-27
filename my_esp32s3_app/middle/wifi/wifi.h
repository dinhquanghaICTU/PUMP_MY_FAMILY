#ifndef __WIFI_H__
#define __WIFI_H__

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_DISCONNECTED_BIT BIT2

typedef enum {
  WIFI_STATE_IDLE = 0,
  WIFI_STATE_CONNECTING,
  WIFI_STATE_CONNECTED,
  WIFI_STATE_DISCONNECTED,
  WIFI_STATE_RECONNECTING,
  WIFI_STATE_FAILED
} wifi_state_t;

typedef struct {
  char ssid[33];
  char password[65];
  int max_retry;
  uint32_t retry_delay_ms;
} app_wifi_config_t;

typedef void (*wifi_event_cb_t)(wifi_state_t state, void *arg);

esp_err_t wifi_init(void);

esp_err_t wifi_connect_sta(const app_wifi_config_t *config);

esp_err_t wifi_disconnect(void);

void wifi_register_callback(wifi_event_cb_t cb, void *user_data);

bool wifi_is_connected(void);

bool wifi_wait_for_connected(TickType_t timeout_ticks);

EventGroupHandle_t wifi_get_event_group(void);

wifi_state_t wifi_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* __WIFI_H__ */
