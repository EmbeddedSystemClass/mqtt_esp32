#include "esp_system.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "cJSON.h"

#include "app_esp32.h"
#include "app_relay.h"

#define MAX_RELAYS 4

extern EventGroupHandle_t mqtt_event_group;
extern const int READY_FOR_REQUEST;

const int relayBase = CONFIG_RELAYS_BASE;
const int relaysNb = CONFIG_RELAYS_NB;
static int relayStatus[MAX_RELAYS];

static const char *TAG = "MQTTS_RELAY";

void relays_init()
{
  for(int i = 0; i < relaysNb; i++) {
    gpio_set_direction( relayBase + i, GPIO_MODE_OUTPUT );
    gpio_set_level(relayBase + i, OFF);
    relayStatus[i] = OFF;
  }
}

void publish_relay_data(esp_mqtt_client_handle_t client)
{

  const char * relays_topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/relays";
  ESP_LOGI(TAG, "waiting READY_FOR_REQUEST in publish_relay_data");
  xEventGroupWaitBits(mqtt_event_group, READY_FOR_REQUEST, true, true, portMAX_DELAY);

  char data[256];
  char relayData[32];
  memset(data,0,256);
  strcat(data, "{\"d\":{");
  for(int i = 0; i < relaysNb; i++) {
    sprintf(relayData, "\"relay%dState\":%d", i, relayStatus[i] == ON);
    if (i != (relaysNb-1)) {
      strcat(relayData, ",");
    }
    strcat(data, relayData);
  }
  strcat(data, "}}");
  int msg_id = esp_mqtt_client_publish(client, relays_topic, data,strlen(data), 0, 0);
  ESP_LOGI(TAG, "sent publish relay successful, msg_id=%d", msg_id);
  xEventGroupSetBits(mqtt_event_group, READY_FOR_REQUEST);

}


int handle_relay_cmd(esp_mqtt_event_handle_t event)
{
  esp_mqtt_client_handle_t client = event->client;
  if (event->data_len >= 32 )
    {
      ESP_LOGI(TAG, "unextected relay cmd payload");
      return -1;
    }
  char tmpBuf[32];
  memcpy(tmpBuf, event->data, event->data_len);
  tmpBuf[event->data_len] = 0;
  cJSON * root   = cJSON_Parse(tmpBuf);
  int id = cJSON_GetObjectItem(root,"id")->valueint;
  int value = cJSON_GetObjectItem(root,"value")->valueint;
  printf("id: %d\r\n", id);
  printf("value: %d\r\n", value);

  if (value == relayStatus[id]) {
    //reversed logic
    if (value == OFF) {
      relayStatus[id] = ON;
    }
    if (value == ON) {
      relayStatus[id] = OFF;
    }
    gpio_set_level(relayBase + id, relayStatus[id]);
    publish_relay_data(client);
  }
  return 0;

}

