#include "esp_system.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "app_esp32.h"
#include "app_relay.h"

#define MAX_RELAYS 4

extern EventGroupHandle_t mqtt_event_group;
extern const int PUBLISHED_BIT;

const int relayBase = CONFIG_RELAYS_BASE;
const int relaysNb = CONFIG_RELAYS_NB;
static int relayStatus[MAX_RELAYS];

static const char *TAG = "MQTTS_RELAY";

extern QueueHandle_t relayQueue;

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
  char data[256];
  char relayData[32];
  memset(data,0,256);
  strcat(data, "{");
  for(int i = 0; i < relaysNb; i++) {
    sprintf(relayData, "\"relay%dState\":%d", i, relayStatus[i] == ON);
    if (i != (relaysNb-1)) {
      strcat(relayData, ",");
    }
    strcat(data, relayData);
  }
  strcat(data, "}");
  
  xEventGroupClearBits(mqtt_event_group, PUBLISHED_BIT);
  int msg_id = esp_mqtt_client_publish(client, relays_topic, data,strlen(data), 1, 0);
  if (msg_id > 0) {
    ESP_LOGI(TAG, "sent publish relay successful, msg_id=%d", msg_id);
    xEventGroupWaitBits(mqtt_event_group, PUBLISHED_BIT, false, true, portMAX_DELAY);
  } else {
    ESP_LOGI(TAG, "failed to publish relay, msg_id=%d", msg_id);
  }

}

void update_relay_state(int id, char value)
{
  if (value == relayStatus[id]) {
    //reversed logic
    if (value == OFF) {
      relayStatus[id] = ON;
    }
    if (value == ON) {
      relayStatus[id] = OFF;
    }
    gpio_set_level(relayBase + id, relayStatus[id]);
  }

}

void handle_relay_cmd_task(void* pvParameters)
{
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;
  struct RelayMessage r;
  int id;
  int value;
  while(1) {
    if( xQueueReceive( relayQueue, &r , portMAX_DELAY) )
      {
        id=r.relayId;
        value=r.relayValue;
        update_relay_state(id, value);
        publish_relay_data(client);

      }
  }
}

