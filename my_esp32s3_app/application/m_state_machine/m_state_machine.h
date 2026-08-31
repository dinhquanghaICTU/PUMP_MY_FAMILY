#ifndef __M_STATE_MACHINE_H__
#define __M_STATE_MACHINE_H__

#include <stdbool.h>

#define MAX_RETRY_COUNT 3

typedef enum {
  STATE_WIFI_CONFIG,
  STATE_WIFI_CONNECT,
  STATE_WIFI_GOT_IP,
  STATE_MQTT_CONNECTING,
  STATE_MQTT_CONNECTED,
  STATE_WIFI_DISCONNECT,
  STATE_WIFI_CONNECT_FAIL,
  STATE_WIFI_START,
  STATE_WIFI_CONNECT_FAILSE,
  STATE_IDLE,
  STATE_OTA,
  STATE_OTA_NODE_TANK
} state_t;

typedef struct {
  state_t state_current;
  state_t state_next;
  bool ble_config_wifi;
  int retry_count;
} m_state_machine_t;

void m_state_machine_task(void *arg);
void m_state_machine_init(void);
void m_state_machine_set_state(state_t state);
bool get_ssid_password(char *ssid_out, char *pass_out);
void m_state_machine_reset_wifi(void);

#endif //__M_STATE_MACHINE_H__