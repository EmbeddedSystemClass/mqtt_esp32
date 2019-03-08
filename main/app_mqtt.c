#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "app_esp32.h"
#include "app_relay.h"
#include "app_ota.h"
#include "app_thermostat.h"
#include "app_mqtt.h"

#include "cJSON.h"



extern EventGroupHandle_t mqtt_event_group;
extern const int CONNECTED_BIT;
extern const int SUBSCRIBED_BIT;
extern const int PUBLISHED_BIT;
extern const int INIT_FINISHED_BIT;


extern int16_t connect_reason;
extern const int mqtt_disconnect;
#define FW_VERSION "0.02.05"

extern QueueHandle_t relayQueue;
extern QueueHandle_t otaQueue;
extern QueueHandle_t thermostatQueue;
extern QueueHandle_t mqttQueue;

static const char *TAG = "MQTTS_MQTTS";



#define MAX_SUB 4
#define RELAY_TOPIC CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/cmd/relay"
#define OTA_TOPIC CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/cmd/ota"
#define THERMOSTAT_TOPIC CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/cmd/thermostat"

const char *SUBSCRIPTIONS[MAX_SUB] =
  {
    RELAY_TOPIC "/0",
    RELAY_TOPIC "/1",
    OTA_TOPIC,
    THERMOSTAT_TOPIC
  };


extern const uint8_t mqtt_iot_cipex_ro_pem_start[] asm("_binary_mqtt_iot_cipex_ro_pem_start");

void dispatch_mqtt_event(esp_mqtt_event_handle_t event)
{

  if (strncmp(event->topic, RELAY_TOPIC, strlen(RELAY_TOPIC)) == 0) {
    char id=255;
    if (strncmp(event->topic, RELAY_TOPIC "/0", strlen(RELAY_TOPIC "/0")) == 0) {
      id=0;
    }
    if (strncmp(event->topic, RELAY_TOPIC "/1", strlen(RELAY_TOPIC "/1")) == 0) {
      id=1;
    }
    if(id == 255)
      {
        ESP_LOGI(TAG, "unexpected relay id");
        return;
      }
    if (event->data_len >= 32 )
      {
        ESP_LOGI(TAG, "unexpected relay cmd payload");
        return;
      }
    char tmpBuf[32];
    memcpy(tmpBuf, event->data, event->data_len);
    tmpBuf[event->data_len] = 0;
    cJSON * root   = cJSON_Parse(tmpBuf);
    char value = cJSON_GetObjectItem(root,"state")->valueint;
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
  if (xEventGroupGetBits(mqtt_event_group) & INIT_FINISHED_BIT)
    {
      const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/connected";
      char data[256];
      memset(data,0,256);

      sprintf(data, "{\"v\":\"" FW_VERSION "\", \"r\":%d}", connect_reason);
      xEventGroupClearBits(mqtt_event_group, PUBLISHED_BIT);
      int msg_id = esp_mqtt_client_publish(client, connect_topic, data,strlen(data), 1, 0);
      if (msg_id > 0) {
        ESP_LOGI(TAG, "sent publish connected data successful, msg_id=%d", msg_id);
        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, PUBLISHED_BIT, false, true, MQTT_FLAG_TIMEOUT);
        if (bits & PUBLISHED_BIT) {
          ESP_LOGI(TAG, "publish ack received, msg_id=%d", msg_id);
        } else {
          ESP_LOGW(TAG, "publish ack not received, msg_id=%d", msg_id);
        }
      } else {
        ESP_LOGW(TAG, "failed to publish connected data, msg_id=%d", msg_id);
      }
    }
}


static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
  switch (event->event_id) {
  case MQTT_EVENT_CONNECTED:
    xEventGroupSetBits(mqtt_event_group, CONNECTED_BIT);
    void * unused;
    if (xQueueSend( mqttQueue
                    ,( void * )&unused
                    ,portMAX_DELAY) != pdPASS) {
      ESP_LOGE(TAG, "Cannot send to relayQueue");
    }
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
    connect_reason=mqtt_disconnect;

    xEventGroupClearBits(mqtt_event_group, CONNECTED_BIT | SUBSCRIBED_BIT | PUBLISHED_BIT | INIT_FINISHED_BIT);
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

static void mqtt_subscribe(esp_mqtt_client_handle_t client)
{
  int msg_id;

  for (int i = 0; i < MAX_SUB; i++) {
    xEventGroupClearBits(mqtt_event_group, SUBSCRIBED_BIT);
    msg_id = esp_mqtt_client_subscribe(client, SUBSCRIPTIONS[i], 0);
    if (msg_id > 0) {
      ESP_LOGI(TAG, "sent subscribe %s successful, msg_id=%d", SUBSCRIPTIONS[i], msg_id);
        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, SUBSCRIBED_BIT, false, true, MQTT_FLAG_TIMEOUT);
        if (bits & SUBSCRIBED_BIT) {
          ESP_LOGI(TAG, "subscribe ack received, msg_id=%d", msg_id);
        } else {
          ESP_LOGW(TAG, "subscribe ack not received, msg_id=%d", msg_id);
        }
    } else {
      ESP_LOGI(TAG, "failed to subscribe %s, msg_id=%d", SUBSCRIPTIONS[i], msg_id);
    }
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
    .keepalive = MQTT_TIMEOUT
  };

  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
  return client;
}


void mqtt_start(esp_mqtt_client_handle_t client)
{
  esp_mqtt_client_start(client);
  xEventGroupWaitBits(mqtt_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
}

void handle_mqtt_sub_pub(void* pvParameters)
{
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;
  void * unused;
  while(1) {
    if( xQueueReceive( mqttQueue, &unused , portMAX_DELAY) )
      {
        xEventGroupClearBits(mqtt_event_group, INIT_FINISHED_BIT);
        mqtt_subscribe(client);
        xEventGroupSetBits(mqtt_event_group, INIT_FINISHED_BIT);
        publish_connected_data(client);
        publish_relay_data(client);
        publish_thermostat_data(client);
        publish_ota_data(client, OTA_READY);

      }
  }
}
