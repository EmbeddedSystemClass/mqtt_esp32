#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"


#include "app_sensors.h"

#include <ds18x20.h>
#include <dht.h>


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

void sensors_read(void* pvParameters)
{


  ds18x20_addr_t addrs[MAX_SENSORS];
  float temps[MAX_SENSORS];
  int sensor_count;

  while (1)
    {
      //FIXME bug when no sensor
      if (dht_read_data(sensor_type, dht_gpio, &humidity, &temperature) == ESP_OK)
        {
          xEventGroupSetBits(sensors_event_group, DHT22);
          ESP_LOGI(TAG, "Humidity: %.1f%% Temp: %.1fC", humidity / 10., temperature / 10.);
        }
      else
        {
          ESP_LOGE(TAG, "Could not read data from sensor\n");
        }
      //END FIXME


      sensor_count = ds18x20_scan_devices(SENSOR_GPIO, addrs, MAX_SENSORS);

      if (sensor_count < 1)
        {
          printf("No sensors detected!\n");
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
          printf("  Sensor %08x%08x reports %f deg C (%f deg F)\n", addr0, addr1, temp_c, temp_f);
          wtemperature = temp_c;
          xEventGroupSetBits(sensors_event_group, DS);
        }
      printf("\n");


      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
