#ifndef __LED_H__
#define __LED_H__

#define LED_PIN 48

typedef enum {
  LED_STATE_OFF = 0,

  LED_STATE_BLE_CONFIG,
  LED_STATE_WIFI_CONNECTING,

  LED_STATE_WIFI_DISCONNECTED,

  LED_STATE_ONLINE_OK,

  LED_STATE_PUMP_RUNNING,

  LED_STATE_OTA_UPDATING,

  LED_STATE_ERROR_TRIPPED

} led_state_t;

void led_init(void);
void led_set_state(led_state_t state);
void led_task(void *pvParam);

#endif //__LED_H__