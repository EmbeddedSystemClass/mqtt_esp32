#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "app_relay.h"
#include "app_ota.h"
#include "app_mqtt_helper.h"
#include "app_mqtt.h"


extern EventGroupHandle_t mqtt_event_group;
extern const int CONNECTED_BIT;
extern const int SUBSCRIBED_BIT;
extern const int READY_FOR_REQUEST;

extern QueueHandle_t xQueue;

static const char *TAG = "MQTTS_MQTTS";


extern const uint8_t messaging_internetofthings_ibmcloud_com_pem_start[] asm("_binary_messaging_internetofthings_ibmcloud_com_pem_start");
extern const uint8_t messaging_internetofthings_ibmcloud_com_pem_end[]   asm("_binary_messaging_internetofthings_ibmcloud_com_pem_end");

void dispatch_mqtt_event(esp_mqtt_event_handle_t event)
{
  if (strncmp(event->topic, "iot-2/cmd/relay/fmt/json", strlen("iot-2/cmd/relay/fmt/json")) == 0) {
    if (handle_relay_cmd(event)) {
      ESP_LOGI(TAG, "cannot handle relay cmd");
    }
  }
  if (strncmp(event->topic, "iot-2/cmd/ota/fmt/json", strlen("iot-2/cmd/ota_update/fmt/json")) == 0) {
    if (handle_ota_update_cmd(event)) {
      ESP_LOGI(TAG, "cannot handle ota_update cmd");
    }

  }
}


static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
  switch (event->event_id) {
  case MQTT_EVENT_CONNECTED:
    xEventGroupSetBits(mqtt_event_group, CONNECTED_BIT | READY_FOR_REQUEST);
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    if (xQueueSend( xQueue
                    ,( void * )&(event->client)
                    ,portMAX_DELAY) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to xqueue");
    }
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
    xEventGroupClearBits(mqtt_event_group, CONNECTED_BIT | SUBSCRIBED_BIT | READY_FOR_REQUEST);
    break;

  case MQTT_EVENT_SUBSCRIBED:
    xEventGroupSetBits(mqtt_event_group, READY_FOR_REQUEST);
    ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    xEventGroupSetBits(mqtt_event_group, READY_FOR_REQUEST);
    ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_PUBLISHED:
    xEventGroupSetBits(mqtt_event_group, READY_FOR_REQUEST);
    ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_DATA:
    ESP_LOGI(TAG, "MQTT_EVENT_DATA");
    ESP_LOGI(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
    ESP_LOGI(TAG, "DATA=%.*s\r\n", event->data_len, event->data);
    dispatch_mqtt_event(event);
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
    break;
  }
  return ESP_OK;
}


static void mqtt_app_start(esp_mqtt_client_handle_t client)
{
  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  esp_mqtt_client_start(client);
}

static void mqtt_manage_device_request(esp_mqtt_client_handle_t client)
{

}

esp_mqtt_client_handle_t mqtt_init()
{
  const esp_mqtt_client_config_t mqtt_cfg = {
    .uri = "mqtts://" CONFIG_MQTT_USERNAME ":" CONFIG_MQTT_PASSWORD "@" CONFIG_MQTT_SERVER,
    .event_handle = mqtt_event_handler,
    .cert_pem = (const char *)messaging_internetofthings_ibmcloud_com_pem_start,
    .client_id = CONFIG_MQTT_CLIENT_ID
  };

  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
  mqtt_app_start(client);
  mqtt_manage_device_request(client);

  xTaskCreate(mqtt_helper, "mqtt_helper", configMINIMAL_STACK_SIZE * 3, (void *)client, 7, NULL);
  ESP_LOGI(TAG, "Waiting for mqtt");
  xEventGroupWaitBits(mqtt_event_group, SUBSCRIBED_BIT, false, true, portMAX_DELAY);

  return client;
}
