#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "dht.h"

#include "app_dht22.h"



extern EventGroupHandle_t sensors_event_group;
extern const int DHT22;

extern int16_t temperature;
extern int16_t humidity;

static const char *TAG = "MQTTS_DHT22";

void dht_read(void* pvParameters)
{
  const dht_sensor_type_t sensor_type = DHT_TYPE_DHT22;
  const gpio_num_t dht_gpio = 25;

  while (1)
    {
      if (dht_read_data(sensor_type, dht_gpio, &humidity, &temperature) == ESP_OK)
        {
          xEventGroupSetBits(sensors_event_group, DHT22);
          ESP_LOGI(TAG, "Humidity: %.1f%% Temp: %.1fC", humidity / 10., temperature / 10.);
        }
      else
        {
          ESP_LOGE(TAG, "Could not read data from sensor\n");
        }
      vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
}
