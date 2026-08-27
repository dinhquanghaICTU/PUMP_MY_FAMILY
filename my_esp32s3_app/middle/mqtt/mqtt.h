#ifndef __MQTT_H__
#define __MQTT_H__

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MQTT_CONNECTED_BIT BIT0

typedef enum {
  MQTT_STATE_DISCONNECTED = 0,
  MQTT_STATE_CONNECTING,
  MQTT_STATE_CONNECTED,
  MQTT_STATE_ERROR
} app_mqtt_state_t;

typedef struct {
  const char *uri;
  const char *username;
  const char *password;
  const char *client_id;
} app_mqtt_config_t;

typedef void (*app_mqtt_data_cb_t)(const char *topic, int topic_len,
                                   const char *data, int data_len, void *arg);

typedef void (*app_mqtt_state_cb_t)(app_mqtt_state_t state, void *arg);
esp_err_t app_mqtt_init(const app_mqtt_config_t *config);
esp_err_t app_mqtt_start(void);
esp_err_t app_mqtt_stop(void);
int app_mqtt_publish(const char *topic, const char *data, int qos, int retain);
int app_mqtt_subscribe(const char *topic, int qos);
void app_mqtt_register_data_cb(app_mqtt_data_cb_t cb, void *arg);
void app_mqtt_register_state_cb(app_mqtt_state_cb_t cb, void *arg);
bool app_mqtt_is_connected(void);
bool app_mqtt_wait_for_connected(TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif

#endif // __MQTT_H__
