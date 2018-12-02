#include "mqtt_client.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "app_mqtt_helper.h"

extern EventGroupHandle_t mqtt_event_group;
extern const int CONNECTED_BIT;
extern const int SUBSCRIBED_BIT;
extern const int READY_FOR_REQUEST;

extern QueueHandle_t xQueue;


static const char *SUBSCRIPTIONS[3] =
  {
    "iotdm-1/mgmt/initiate/device/reboot",
    "iot-2/cmd/relay/fmt/json",
    "iot-2/cmd/ota/fmt/json"
  };


static const char *TAG = "MQTTS_MQTT_HELPER";

static void mqtt_subscribe(esp_mqtt_client_handle_t client)
{
  int msg_id;

  for (int i = 0; i < 3; i++) {
    ESP_LOGI(TAG, "waiting READY_FOR_REQUEST in mqtt_subscribe");
    xEventGroupWaitBits(mqtt_event_group, READY_FOR_REQUEST, true, true, portMAX_DELAY);
    msg_id = esp_mqtt_client_subscribe(client, SUBSCRIPTIONS[i], 0);
    ESP_LOGI(TAG, "sent subscribe %s successful, msg_id=%d", SUBSCRIPTIONS[i], msg_id);
  }
  xEventGroupSetBits(mqtt_event_group, SUBSCRIBED_BIT);
}

void mqtt_helper(void* pvParameters)
{
  esp_mqtt_client_handle_t client;
  ESP_LOGI(TAG, "starting mqtt_helper task");
  while(1) {
    if( xQueueReceive( xQueue, &client , portMAX_DELAY) )
      {
        // pcRxedMessage now points to the struct AMessage variable posted
        // by vATask.
        mqtt_subscribe(client);
      }
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

