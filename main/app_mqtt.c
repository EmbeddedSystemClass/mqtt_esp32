#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "app_relay.h"
#include "app_ota.h"
#include "app_thermostat.h"
#include "app_mqtt.h"

#include "cJSON.h"



extern EventGroupHandle_t mqtt_event_group;
extern const int CONNECTED_BIT;
extern const int SUBSCRIBED_BIT;
extern const int PUBLISHED_BIT;


extern int16_t connect_reason;
extern const int mqtt_disconnect;
#define FW_VERSION "0.02.05"

QueueHandle_t relayQueue;
QueueHandle_t otaQueue;
QueueHandle_t thermostatQueue;

static const char *TAG = "MQTTS_MQTTS";



#define MAX_SUB 3
#define RELAY_TOPIC CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/cmd/relay"
#define OTA_TOPIC CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/cmd/ota"
#define THERMOSTAT_TOPIC CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/cmd/thermostat"

const char *SUBSCRIPTIONS[MAX_SUB] =
  {
    RELAY_TOPIC,
    OTA_TOPIC,
    THERMOSTAT_TOPIC
  };


extern const uint8_t mqtt_iot_cipex_ro_pem_start[] asm("_binary_mqtt_iot_cipex_ro_pem_start");

void dispatch_mqtt_event(esp_mqtt_event_handle_t event)
{

  if (strncmp(event->topic, RELAY_TOPIC, strlen(RELAY_TOPIC)) == 0) {

    if (event->data_len >= 32 )
      {
        ESP_LOGI(TAG, "unextected relay cmd payload");
        return;
      }
    char tmpBuf[32];
    memcpy(tmpBuf, event->data, event->data_len);
    tmpBuf[event->data_len] = 0;
    cJSON * root   = cJSON_Parse(tmpBuf);
    char id = cJSON_GetObjectItem(root,"id")->valueint;
    char value = cJSON_GetObjectItem(root,"value")->valueint;
    printf("id: %d\r\n", id);
    printf("value: %d\r\n", value);
    struct RelayMessage r={id, value};
    if (xQueueSend( relayQueue
                    ,( void * )&r
                    ,portMAX_DELAY) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to relayQueue");
    }
  }
  if (strncmp(event->topic, OTA_TOPIC, strlen(OTA_TOPIC)) == 0) {
    struct OtaMessage o={"https://sw.iot.cipex.ro:8911/" CONFIG_MQTT_CLIENT_ID ".bin"};
    if (xQueueSend( otaQueue
                    ,( void * )&o
                    ,portMAX_DELAY) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to relayQueue");

    }
  }

  if (strncmp(event->topic, THERMOSTAT_TOPIC, strlen(THERMOSTAT_TOPIC)) == 0) {
    if (event->data_len >= 64 )
      {
        ESP_LOGI(TAG, "unextected relay cmd payload");
        return;
      }
    char tmpBuf[64];
    memcpy(tmpBuf, event->data, event->data_len);
    tmpBuf[event->data_len] = 0;
    cJSON * root   = cJSON_Parse(tmpBuf);
    float targetTemperature = cJSON_GetObjectItem(root,"targetTemperature")->valuedouble;
    printf("targetTemperature: %f\r\n", targetTemperature);
    struct ThermostatMessage t={targetTemperature};
    if (xQueueSend( thermostatQueue
                    ,( void * )&t
                    ,portMAX_DELAY) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to thermostatQueue");
    }

  }
}

void publish_connected_data(esp_mqtt_client_handle_t client)
{

  const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/connected";
  char data[256];
  memset(data,0,256);

  sprintf(data, "{\"v\":\"" FW_VERSION "\", \"r\":%d}", connect_reason);
  xEventGroupClearBits(mqtt_event_group, PUBLISHED_BIT);
  int msg_id = esp_mqtt_client_publish(client, connect_topic, data,strlen(data), 1, 0);
  ESP_LOGI(TAG, "sent publish connected data successful, msg_id=%d", msg_id);
  xEventGroupWaitBits(mqtt_event_group, PUBLISHED_BIT, false, true, portMAX_DELAY);

}


static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
  switch (event->event_id) {
  case MQTT_EVENT_CONNECTED:
    xEventGroupSetBits(mqtt_event_group, CONNECTED_BIT);
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
    connect_reason=mqtt_disconnect;

    xEventGroupClearBits(mqtt_event_group, CONNECTED_BIT | SUBSCRIBED_BIT | PUBLISHED_BIT);
    break;

  case MQTT_EVENT_SUBSCRIBED:
    xEventGroupSetBits(mqtt_event_group, SUBSCRIBED_BIT);
    ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_PUBLISHED:
    xEventGroupSetBits(mqtt_event_group, PUBLISHED_BIT);
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



static esp_mqtt_client_handle_t mqtt_app_init_start(const esp_mqtt_client_config_t* mqtt_cfg)
{
  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  esp_mqtt_client_handle_t client = esp_mqtt_client_init(mqtt_cfg);
  esp_mqtt_client_start(client);
  xEventGroupWaitBits(mqtt_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
  return client;
}

static void mqtt_subscribe(esp_mqtt_client_handle_t client)
{
  int msg_id;

  for (int i = 0; i < MAX_SUB; i++) {
    xEventGroupClearBits(mqtt_event_group, SUBSCRIBED_BIT);
    msg_id = esp_mqtt_client_subscribe(client, SUBSCRIPTIONS[i], 0);
    ESP_LOGI(TAG, "sent subscribe %s successful, msg_id=%d", SUBSCRIPTIONS[i], msg_id);
    xEventGroupWaitBits(mqtt_event_group, SUBSCRIBED_BIT, false, true, portMAX_DELAY);
  }
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

  esp_mqtt_client_handle_t client = mqtt_app_init_start(&mqtt_cfg);
  thermostatQueue = xQueueCreate(1, sizeof(struct ThermostatMessage) );
  relayQueue = xQueueCreate(1, sizeof(struct RelayMessage) );
  otaQueue = xQueueCreate(1, sizeof(struct OtaMessage) );

  mqtt_subscribe(client);
  publish_connected_data(client);


  return client;
}
