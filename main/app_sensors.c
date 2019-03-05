#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"


#include "app_mqtt.h"
extern EventGroupHandle_t mqtt_event_group;
extern const int PUBLISHED_BIT;

#include "app_sensors.h"

#include <ds18x20.h>
#include <dht.h>
bool heatEnabled = false;



extern EventGroupHandle_t sensors_event_group;
extern const int DHT22;
extern const int DS;

extern float wtemperature;

extern int16_t temperature;
extern int16_t humidity;

static const char *TAG = "MQTTS_DHT22";


static const gpio_num_t SENSOR_GPIO = CONFIG_DS18X20_GPIO;
static const uint32_t LOOP_DELAY_MS = 250;
static const int MAX_SENSORS = 8;
static const int RESCAN_INTERVAL = 8;

const dht_sensor_type_t sensor_type = DHT_TYPE_DHT22;
const gpio_num_t dht_gpio = CONFIG_DHT_GPIO;



void updateHeatingState(bool heatEnabled)
{
  //FIXME update heat state switch/relay and publish it to mqtt
}


void mqtt_publish_sensor_data(void* pvParameters)
{
  const char * sensors_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/sensors";
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;
  ESP_LOGI(TAG, "starting mqtt_publish_sensor_data");
  int msg_id;

  int targetWaterTemp=30*10; //30 degrees
  int targetTemperatureSensibility=5; //0.5 degrees

  if (heatEnabled==true && wtemperature > targetWaterTemp + targetTemperatureSensibility)
    {
      heatEnabled=false;
      updateHeatingState(heatEnabled);
    }


  if (heatEnabled==false && wtemperature < targetWaterTemp - targetTemperatureSensibility)
    {
      heatEnabled=true;
      updateHeatingState(heatEnabled);
    }

  char data[256];
  memset(data,0,256);
  sprintf(data, "{\"counter\":%lld, \"humidity\":%.1f, \"temperature\":%.1f, \"wtemperature\":%.1f}",esp_timer_get_time(),humidity / 10., temperature / 10., wtemperature);

  xEventGroupClearBits(mqtt_event_group, PUBLISHED_BIT);
  msg_id = esp_mqtt_client_publish(client, sensors_topic, data,strlen(data), 1, 0);
  ESP_LOGI(TAG, "sent publish temp successful, msg_id=%d", msg_id);
  xEventGroupWaitBits(mqtt_event_group, PUBLISHED_BIT, false, true, portMAX_DELAY);
}


void sensors_read(void* pvParameters)
{


  ds18x20_addr_t addrs[MAX_SENSORS];
  float temps[MAX_SENSORS];
  int sensor_count;

  while (1)
    {
      if (dht_read_data(sensor_type, dht_gpio, &humidity, &temperature) == ESP_OK)
        {
          ESP_LOGI(TAG, "Humidity: %.1f%% Temp: %.1fC", humidity / 10., temperature / 10.);
        }
      else
        {
          ESP_LOGE(TAG, "Could not read data from sensor\n");
        }


      sensor_count = ds18x20_scan_devices(SENSOR_GPIO, addrs, MAX_SENSORS);

      if (sensor_count < 1)
        {
          ESP_LOGW(TAG, "No sensors detected!\n");
        }

      ds18x20_measure_and_read_multi(SENSOR_GPIO, addrs, sensor_count, temps);
      for (int j = 0; j < sensor_count; j++)
        {
          // The ds18x20 address is a 64-bit integer, but newlib-nano
          // printf does not support printing 64-bit values, so we
          // split it up into two 32-bit integers and print them
          // back-to-back to make it look like one big hex number.
          uint32_t addr0 = addrs[j] >> 32;
          uint32_t addr1 = addrs[j];
          float temp_c = temps[j];
          float temp_f = (temp_c * 1.8) + 32;
          ESP_LOGI(TAG,"  Sensor %08x%08x reports %f deg C (%f deg F)\n", addr0, addr1, temp_c, temp_f);
          wtemperature = temp_c;
        }

      mqtt_publish_sensor_data(pvParameters);

      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
