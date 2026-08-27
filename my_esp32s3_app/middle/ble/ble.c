#include "ble.h"
#include "esp_log.h"
#include "flash.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <string.h>

static const char *TAG = "MIDDLE_BLE";

static char s_device_name[32] = "ESP32_PUMP_CONFIG";
static char s_wifi_ssid[MAX_SSID_LEN] = {0};
static char s_wifi_pass[MAX_PASS_LEN] = {0};
static ble_wifi_config_cb_t s_config_cb = NULL;
static uint8_t s_own_addr_type;

static int gatt_svr_chr_access_wifi(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt,
                                    void *arg);
static void ble_app_advertise(void);

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_WIFI_SVC_UUID),
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = BLE_UUID16_DECLARE(BLE_WIFI_CHR_SSID_UUID),
                    .access_cb = gatt_svr_chr_access_wifi,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(BLE_WIFI_CHR_PASS_UUID),
                    .access_cb = gatt_svr_chr_access_wifi,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                },
                {
                    0,
                }},
    },
    {
        0,
    },
};

static int gatt_svr_chr_access_wifi(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt,
                                    void *arg) {
  uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);

  switch (ctxt->op) {
  case BLE_GATT_ACCESS_OP_READ_CHR:
    if (uuid16 == BLE_WIFI_CHR_SSID_UUID) {
      return os_mbuf_append(ctxt->om, s_wifi_ssid, strlen(s_wifi_ssid));
    } else if (uuid16 == BLE_WIFI_CHR_PASS_UUID) {
      return os_mbuf_append(ctxt->om, s_wifi_pass, strlen(s_wifi_pass));
    }
    return BLE_ATT_ERR_UNLIKELY;

  case BLE_GATT_ACCESS_OP_WRITE_CHR: {

    /*
      handle data form ble-central

    */
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (uuid16 == BLE_WIFI_CHR_SSID_UUID) {
      if (len >= MAX_SSID_LEN)
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
      memset(s_wifi_ssid, 0, sizeof(s_wifi_ssid));
      os_mbuf_copydata(ctxt->om, 0, len, s_wifi_ssid);
      ESP_LOGI(TAG, "BLE da nhan SSID: %s", s_wifi_ssid);
    } else if (uuid16 == BLE_WIFI_CHR_PASS_UUID) {
      if (len >= MAX_PASS_LEN)
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
      memset(s_wifi_pass, 0, sizeof(s_wifi_pass));
      os_mbuf_copydata(ctxt->om, 0, len, s_wifi_pass);
      ESP_LOGI(TAG, "BLE da nhan PASSWORD: %s", s_wifi_pass);

      if (strlen(s_wifi_ssid) > 0) {
        nvs_save_wifi_credentials(s_wifi_ssid, s_wifi_pass);

        if (s_config_cb) {
          s_config_cb(s_wifi_ssid, s_wifi_pass);
        }
      }
    }
    return 0;
  }

  default:
    return BLE_ATT_ERR_UNLIKELY;
  }
}

static int ble_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT:
    ESP_LOGI(TAG, "Dien thoai da ket noi BLE! Status=%d",
             event->connect.status);
    if (event->connect.status != 0) {
      ble_app_advertise();
    }
    break;

  case BLE_GAP_EVENT_DISCONNECT:
    ESP_LOGI(TAG, "Dien thoai da ngat ket noi BLE -> Phat lai Advertising...");
    ble_app_advertise();
    break;

  case BLE_GAP_EVENT_ADV_COMPLETE:
    ESP_LOGI(TAG, "Advertising complete -> Restarting");
    ble_app_advertise();
    break;

  default:
    break;
  }
  return 0;
}

/*

  phát gói tin quảng bá

*/
static void ble_app_advertise(void) {
  struct ble_gap_adv_params adv_params;
  struct ble_hs_adv_fields fields;
  int rc;

  memset(&fields, 0, sizeof(fields));
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.tx_pwr_lvl_is_present = 1;
  fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
  fields.name = (uint8_t *)s_device_name;
  fields.name_len = strlen(s_device_name);
  fields.name_is_complete = 1;

  /*
    nếu phát đi false

  */
  rc = ble_gap_adv_set_fields(&fields);
  if (rc != 0) {
    ESP_LOGE(TAG, "Error setting advertisement data; rc=%d", rc);
    return;
  }

  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

  rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                         ble_gap_event, NULL);
  if (rc != 0) {
    ESP_LOGE(TAG, "Error enabling advertisement; rc=%d", rc);
    return;
  }
  ESP_LOGI(TAG, "Dang phat BLE Advertising: [%s] ...", s_device_name);
}

static void ble_on_sync(void) {
  ble_hs_id_infer_auto(0, &s_own_addr_type);
  ble_app_advertise();
}

static void ble_host_task(void *param) {
  nimble_port_run();
  nimble_port_freertos_deinit();
}

esp_err_t ble_wifi_init(const char *device_name) {
  if (device_name && strlen(device_name) > 0) {
    strncpy(s_device_name, device_name, sizeof(s_device_name) - 1);
  }

  esp_err_t ret = nimble_port_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init nimble %d", ret);
    return ret;
  }

  ble_svc_gap_device_name_set(s_device_name);

  ble_svc_gap_init();
  ble_svc_gatt_init();
  ble_gatts_count_cfg(s_gatt_svcs);
  ble_gatts_add_svcs(s_gatt_svcs);

  ble_hs_cfg.sync_cb = ble_on_sync;

  nimble_port_freertos_init(ble_host_task);
  return ESP_OK;
}

void ble_wifi_register_callback(ble_wifi_config_cb_t cb) { s_config_cb = cb; }

esp_err_t ble_wifi_stop(void) { return nimble_port_stop(); }

void ble_wifi_get_credentials(char *ssid_out, char *pass_out) {
  if (ssid_out)
    strcpy(ssid_out, s_wifi_ssid);
  if (pass_out)
    strcpy(pass_out, s_wifi_pass);
}
