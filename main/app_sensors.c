#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "dht.h"
#include "ds18b20.h"

#include "app_sensors.h"


extern EventGroupHandle_t sensors_event_group;
extern const int DHT22;
extern const int DS;

extern float wtemperature;

extern int16_t temperature;
extern int16_t humidity;

static const char *TAG = "MQTTS_DHT22";

void sensors_read(void* pvParameters)
{
  const dht_sensor_type_t sensor_type = DHT_TYPE_DHT22;
  const gpio_num_t dht_gpio = 22;
  const gpio_num_t DS_PIN = 21;
  ds18b20_init(DS_PIN);

  while (1)
    {
      //FIXME bug when no sensor
      /* if (dht_read_data(sensor_type, dht_gpio, &humidity, &temperature) == ESP_OK) */
      /*   { */
      /*     xEventGroupSetBits(sensors_event_group, DHT22); */
      /*     ESP_LOGI(TAG, "Humidity: %.1f%% Temp: %.1fC", humidity / 10., temperature / 10.); */
      /*   } */
      /* else */
      /*   { */
      /*     ESP_LOGE(TAG, "Could not read data from sensor\n"); */
      /*   } */
      //END FIXME
     wtemperature = ds18b20_get_temp();
     if (-55. < wtemperature && wtemperature < 125. ) {
       xEventGroupSetBits(sensors_event_group, DS);
       xEventGroupSetBits(sensors_event_group, DHT22); //FIXME
     }
     ESP_LOGI(TAG, "Water temp: %.1fC", wtemperature);
      vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
}
