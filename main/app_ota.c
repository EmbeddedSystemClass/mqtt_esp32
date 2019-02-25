#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "cJSON.h"

#include "app_ota.h"

static const char *TAG = "MQTTS_OTA";

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
  switch(evt->event_id) {
  case HTTP_EVENT_ERROR:
    ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
    break;
  case HTTP_EVENT_ON_CONNECTED:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
    break;
  case HTTP_EVENT_HEADER_SENT:
    ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
    break;
  case HTTP_EVENT_ON_HEADER:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
    break;
  case HTTP_EVENT_ON_DATA:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
    break;
  case HTTP_EVENT_ON_FINISH:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
    break;
  case HTTP_EVENT_DISCONNECTED:
    ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
    break;
  }
  return ESP_OK;
}

int handle_ota_update_cmd(esp_mqtt_event_handle_t event)
{
  //  esp_mqtt_client_handle_t client = event->client;
  if (event->data_len >= 128 )
    {
      ESP_LOGI(TAG, "unexpected ota cmd payload");
      return -1;
    }
  char tmpBuf[128];
  memcpy(tmpBuf, event->data, event->data_len);
  char * url = "https://sw.iot.cipex.ro:8911/" CONFIG_MQTT_CLIENT_ID ".bin";

  printf("url: %s\r\n", url);
  static const char *TAG = "simple_ota_example";

  ESP_LOGI(TAG, "Starting OTA example...");
  extern const uint8_t server_cert_pem_start[] asm("_binary_sw_iot_cipex_ro_pem_start");

  esp_http_client_config_t config = {
    .url = url,
    .cert_pem = (char *)server_cert_pem_start,
    .event_handler = _http_event_handler,
  };
  esp_err_t ret = esp_https_ota(&config);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Firmware Upgrades Success, will restart in 10 seconds");
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    esp_restart();
  } else {
    ESP_LOGE(TAG, "Firmware Upgrades Failed");
  }
  return true;
}

