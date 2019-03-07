#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "nvs.h"

#include "app_thermostat.h"



bool heatEnabled = false;
int targetTemperature=23*10; //30 degrees
int targetTemperatureSensibility=5; //0.5 degrees

extern float wtemperature;
extern EventGroupHandle_t mqtt_event_group;
extern const int PUBLISHED_BIT;

extern QueueHandle_t thermostatQueue;


static const char *TAG = "APP_THERMOSTAT";

void publish_thermostat_data(esp_mqtt_client_handle_t client)
{

  const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/thermostat";
  char data[256];
  memset(data,0,256);

  sprintf(data, "{\"targetTemperature\":%02f}", targetTemperature/10.);
  xEventGroupClearBits(mqtt_event_group, PUBLISHED_BIT);
  int msg_id = esp_mqtt_client_publish(client, connect_topic, data,strlen(data), 1, 0);
  ESP_LOGI(TAG, "sent publish thermostat data successful, msg_id=%d", msg_id);
  xEventGroupWaitBits(mqtt_event_group, PUBLISHED_BIT, false, true, portMAX_DELAY);

}

esp_err_t write_thermostat_nvs()
{
  nvs_handle my_handle;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  } else {
    printf("Done\n");
    printf("Updating targetTemperature in NVS ... ");
    err = nvs_set_i32(my_handle, "targetTemp", targetTemperature);
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
  }
  return err;
}

esp_err_t read_thermostat_nvs()
{
  printf("Opening Non-Volatile Storage (NVS) handle... ");
  nvs_handle my_handle;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  } else {
    printf("Done\n");

    // Read
    printf("Reading targetTemperature from NVS ... ");
    err = nvs_get_i32(my_handle, "targetTemp", &targetTemperature);
    switch (err) {
    case ESP_OK:
      printf("Done\n");
      printf("targetTemperature = %d\n", targetTemperature);
      break;
    case ESP_ERR_NVS_NOT_FOUND:
      printf("The value is not initialized yet!\n");
      err = ESP_OK;
      break;
    default :
      printf("Error (%s) reading!\n", esp_err_to_name(err));
    }
  }

  return err;
}

void updateHeatingState(bool heatEnabled)
{
  ESP_LOGI(TAG, "heat state updated to %d", heatEnabled);
}


void update_thermostat()
{

  ESP_LOGI(TAG, "heat state is %d", heatEnabled);
  ESP_LOGI(TAG, "wtemperature is %f", wtemperature);
  ESP_LOGI(TAG, "targetTemperature is %d", targetTemperature);
  ESP_LOGI(TAG, "targetTemperatureSensibility is %d", targetTemperatureSensibility);
  if ( wtemperature == -1 )
    {
      ESP_LOGI(TAG, "sensor is not reporting, stop heating as we are blind");
      if (heatEnabled==true) {
        heatEnabled=false;
        updateHeatingState(heatEnabled);
      }
    }

  if (heatEnabled==true && wtemperature * 10 > targetTemperature + targetTemperatureSensibility)
    {
      heatEnabled=false;
      updateHeatingState(heatEnabled);
    }


  if (heatEnabled==false && wtemperature  * 10 < targetTemperature - targetTemperatureSensibility)
    {
      heatEnabled=true;
      updateHeatingState(heatEnabled);
    }

}

void handle_thermostat_cmd_task(void* pvParameters)
{
  //esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;
  struct ThermostatMessage t;
  while(1) {
    if( xQueueReceive( thermostatQueue, &t , portMAX_DELAY) )
      {
        if (targetTemperature != t.targetTemperature * 10) {
          targetTemperature=t.targetTemperature*10;
          esp_err_t err = write_thermostat_nvs();
          ESP_ERROR_CHECK( err );
          update_thermostat();
        }
      }
  }

}
