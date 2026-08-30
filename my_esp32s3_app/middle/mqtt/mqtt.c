#include "mqtt.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include <string.h>

static const char *TAG = "MIDDLE_MQTT";

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static EventGroupHandle_t s_mqtt_event_group = NULL;
static app_mqtt_state_t s_mqtt_state = MQTT_STATE_DISCONNECTED;
static app_mqtt_data_cb_t s_data_callback = NULL;
static void *s_data_arg = NULL;
static app_mqtt_state_cb_t s_state_callback = NULL;
static void *s_state_arg = NULL;

static void set_mqtt_state(app_mqtt_state_t state) {
  s_mqtt_state = state;
  if (s_state_callback) {
    s_state_callback(state, s_state_arg);
  }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "MQTT Connected to HiveMQ Broker!");
    set_mqtt_state(MQTT_STATE_CONNECTED);
    xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
    
    // Tự động Subscribe / Re-Subscribe lại các topic điều khiển
    esp_mqtt_client_subscribe(s_mqtt_client, "pump/family/command", 1);
    esp_mqtt_client_subscribe(s_mqtt_client, "pump/family/ota", 1);
    break;

  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGW(TAG, "MQTT Disconnected from Broker!");
    set_mqtt_state(MQTT_STATE_DISCONNECTED);
    xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
    break;

  case MQTT_EVENT_SUBSCRIBED:
    ESP_LOGI(TAG, "Subscribed successfully, msg_id=%d", event->msg_id);
    break;

  case MQTT_EVENT_DATA:
    ESP_LOGI(TAG, "Received topic: %.*s, data: %.*s", event->topic_len,
             event->topic, event->data_len, event->data);
    if (s_data_callback) {
      s_data_callback(event->topic, event->topic_len, event->data,
                      event->data_len, s_data_arg);
    }
    break;

  case MQTT_EVENT_ERROR:
    ESP_LOGE(TAG, "MQTT Event Error");
    set_mqtt_state(MQTT_STATE_ERROR);
    break;

  default:
    break;
  }
}

esp_err_t app_mqtt_init(const app_mqtt_config_t *config) {
  if (!config || !config->uri) {
    return ESP_ERR_INVALID_ARG;
  }

  // Nếu đã khởi tạo rồi thì không tạo mới để tránh chạy song song 2 client trùng Client ID
  if (s_mqtt_client != NULL) {
    return ESP_OK;
  }

  if (!s_mqtt_event_group) {
    s_mqtt_event_group = xEventGroupCreate();
  }

  esp_mqtt_client_config_t mqtt_cfg = {
      .broker.address.uri = config->uri,
      .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
      .credentials.username = config->username,
      .credentials.authentication.password = config->password,
      .credentials.client_id = config->client_id,
      .session.keepalive = 60,
  };

  s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  if (!s_mqtt_client) {
    ESP_LOGE(TAG, "Failed to initialize MQTT client");
    return ESP_FAIL;
  }

  return esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID,
                                        mqtt_event_handler, NULL);
}

esp_err_t app_mqtt_start(void) {
  if (!s_mqtt_client)
    return ESP_ERR_INVALID_STATE;
  
  if (s_mqtt_state == MQTT_STATE_CONNECTED) {
    return ESP_OK;
  }

  set_mqtt_state(MQTT_STATE_CONNECTING);
  return esp_mqtt_client_start(s_mqtt_client);
}

esp_err_t app_mqtt_stop(void) {
  if (!s_mqtt_client)
    return ESP_ERR_INVALID_STATE;
  return esp_mqtt_client_stop(s_mqtt_client);
}

int app_mqtt_publish(const char *topic, const char *data, int qos, int retain) {
  if (!s_mqtt_client || s_mqtt_state != MQTT_STATE_CONNECTED)
    return -1;
  return esp_mqtt_client_publish(s_mqtt_client, topic, data, 0, qos, retain);
}

int app_mqtt_subscribe(const char *topic, int qos) {
  if (!s_mqtt_client || s_mqtt_state != MQTT_STATE_CONNECTED)
    return -1;
  return esp_mqtt_client_subscribe(s_mqtt_client, topic, qos);
}

void app_mqtt_register_data_cb(app_mqtt_data_cb_t cb, void *arg) {
  s_data_callback = cb;
  s_data_arg = arg;
}

void app_mqtt_register_state_cb(app_mqtt_state_cb_t cb, void *arg) {
  s_state_callback = cb;
  s_state_arg = arg;
}

bool app_mqtt_is_connected(void) {
  return (s_mqtt_state == MQTT_STATE_CONNECTED);
}

bool app_mqtt_wait_for_connected(TickType_t timeout_ticks) {
  if (!s_mqtt_event_group)
    return false;
  EventBits_t bits = xEventGroupWaitBits(s_mqtt_event_group, MQTT_CONNECTED_BIT,
                                         pdFALSE, pdFALSE, timeout_ticks);
  return (bits & MQTT_CONNECTED_BIT) != 0;
}
