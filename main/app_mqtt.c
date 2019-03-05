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


extern int16_t connect_reason;
extern const int mqtt_disconnect;
#define FW_VERSION "0.02.05"

extern QueueHandle_t xQueue;

static const char *TAG = "MQTTS_MQTTS";

extern const uint8_t mqtt_iot_cipex_ro_pem_start[] asm("_binary_mqtt_iot_cipex_ro_pem_start");

void dispatch_mqtt_event(esp_mqtt_event_handle_t event)
{

  if (strncmp(event->topic, RELAY_TOPIC, strlen(RELAY_TOPIC)) == 0) {
    if (handle_relay_cmd(event)) {
      ESP_LOGI(TAG, "cannot handle relay cmd");
    }
  }
  if (strncmp(event->topic, OTA_TOPIC, strlen(OTA_TOPIC)) == 0) {
    if (handle_ota_update_cmd(event)) {
      ESP_LOGI(TAG, "cannot handle ota_update cmd");
    }

  }
}

void publish_connected_data(esp_mqtt_client_handle_t client)
{

  const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/connected";
  ESP_LOGI(TAG, "waiting READY_FOR_REQUEST in publish_connected_data");
  xEventGroupWaitBits(mqtt_event_group, READY_FOR_REQUEST, true, true, portMAX_DELAY);
  char data[256];
  memset(data,0,256);

  sprintf(data, "{\"v\":\"" FW_VERSION "\", \"r\":%d}", connect_reason);
  int msg_id = esp_mqtt_client_publish(client, connect_topic, data,strlen(data), 0, 0);
  ESP_LOGI(TAG, "sent publish relay successful, msg_id=%d", msg_id);
  xEventGroupSetBits(mqtt_event_group, READY_FOR_REQUEST);

}


static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
  switch (event->event_id) {
  case MQTT_EVENT_CONNECTED:
    xEventGroupSetBits(mqtt_event_group, CONNECTED_BIT | READY_FOR_REQUEST);
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    publish_connected_data(event->client);
            
    if (xQueueSend( xQueue
                    ,( void * )&(event->client)
                    ,portMAX_DELAY) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to xqueue");
    }
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
    connect_reason=mqtt_disconnect;

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
    .cert_pem = (const char *)mqtt_iot_cipex_ro_pem_start,
    .client_id = CONFIG_MQTT_CLIENT_ID,
    .lwt_topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/disconnected",
    .keepalive = 60
  };

  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
  mqtt_app_start(client);
  mqtt_manage_device_request(client);

  xTaskCreate(mqtt_helper, "mqtt_helper", configMINIMAL_STACK_SIZE * 3, (void *)client, 7, NULL);
  ESP_LOGI(TAG, "Waiting for mqtt");
  xEventGroupWaitBits(mqtt_event_group, SUBSCRIBED_BIT, false, true, portMAX_DELAY);

  ESP_LOGI(TAG, "waiting READY_FOR_REQUEST in mqtt_init");
  return client;
}
