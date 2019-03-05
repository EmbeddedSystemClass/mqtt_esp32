#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "esp_log.h"

extern "C" {
#include "app_esp32.h"
#include "app_relay.h"
#include "app_wifi.h"
#include "app_sensors.h"
#include "app_mqtt.h"
}

//#include "app_tft.h"

static const char *TAG = "MQTTS_MAIN";

EventGroupHandle_t wifi_event_group;
EventGroupHandle_t mqtt_event_group;
extern "C" const int CONNECTED_BIT = BIT0;
extern "C" const int SUBSCRIBED_BIT = BIT1;
extern "C" const int PUBLISHED_BIT = BIT2;

EventGroupHandle_t sensors_event_group;
extern "C" const int DHT22 = BIT0;
extern "C" const int DS = BIT1;


float wtemperature = 0;

int16_t temperature = 0;
int16_t humidity = 0;


int16_t connect_reason;
const int boot = 0;
extern "C" const int mqtt_disconnect = 1;


#define BLINK_GPIO GPIO_NUM_27
void blink_task(void *pvParameter)
{
  /* Configure the IOMUX register for pad BLINK_GPIO (some pads are
     muxed to GPIO on reset already, but some default to other
     functions and need to be switched to GPIO. Consult the
     Technical Reference for a list of pads and their default
     functions.)
  */

  gpio_pad_select_gpio(BLINK_GPIO);
  /* Set the GPIO as a push/pull output */
  gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
  while(1) {
    xEventGroupWaitBits(mqtt_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    gpio_set_level(BLINK_GPIO, ON);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(BLINK_GPIO, OFF);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// void updateHeatingState(bool heatEnabled)
// {
//   //FIXME update heat state switch/relay and publish it to mqtt
// }


// void mqtt_publish_sensor_data(void* pvParameters)
// {
//   const char * sensors_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/sensors";
//   esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;
//   ESP_LOGI(TAG, "starting mqtt_publish_sensor_data");
//   int msg_id;

//   int targetWaterTemp=30*10; //30 degrees
//   int targetTemperatureSensibility=5; //0.5 degrees

//   while(1) {
//     ESP_LOGI(TAG, "waiting DHT22 | DS in mqtt_publish_sensor_data");
//     // xEventGroupWaitBits(sensors_event_group, DHT22 | DS, true, true, portMAX_DELAY);

//     if (heatEnabled==true && wtemperature > targetWaterTemp + targetTemperatureSensibility)
//       {
//         heatEnabled=false;
//         updateHeatingState(heatEnabled);
//       }


//     if (heatEnabled==false && wtemperature < targetWaterTemp - targetTemperatureSensibility)
//       {
//         heatEnabled=true;
//         updateHeatingState(heatEnabled);
//       }

//     ESP_LOGI(TAG, "waiting READY_FOR_REQUEST in mqtt_publish_sensor_data");
//     // xEventGroupWaitBits(mqtt_event_group, READY_FOR_REQUEST, true, true, portMAX_DELAY);
//     char data[256];
//     memset(data,0,256);
//     sprintf(data, "{\"counter\":%lld, \"humidity\":%.1f, \"temperature\":%.1f, \"wtemperature\":%.1f}",esp_timer_get_time(),humidity / 10., temperature / 10., wtemperature);
//     msg_id = esp_mqtt_client_publish(client, sensors_topic, data,strlen(data), 0, 0);
//     ESP_LOGI(TAG, "sent publish temp successful, msg_id=%d", msg_id);
//     // xEventGroupSetBits(mqtt_event_group, READY_FOR_REQUEST);
//   }
// }




extern "C" void app_main()
{
  connect_reason=boot;
  ESP_LOGI(TAG, "[APP] Startup..");
  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
  esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

 
  relays_init();

  mqtt_event_group = xEventGroupCreate();
  sensors_event_group = xEventGroupCreate();
  wifi_event_group = xEventGroupCreate();

  xTaskCreate(blink_task, "blink_task", configMINIMAL_STACK_SIZE, NULL, 3, NULL);
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK( err );
 
  wifi_init();
  esp_mqtt_client_handle_t client = mqtt_init();
  publish_relay_data(client);

  xTaskCreate(sensors_read, "sensors_read", configMINIMAL_STACK_SIZE * 3, (void *)client, 10, NULL);
  xTaskCreate(handle_relay_cmd_task, "handle_relay_cmd_task", configMINIMAL_STACK_SIZE * 3, (void *)client, 5, NULL);


  //xTaskCreate(tft_handler, "tft_handler", configMINIMAL_STACK_SIZE * 3, NULL, 7, NULL);
}
