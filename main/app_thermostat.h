#ifndef APP_THERMOSTAT_H
#define APP_THERMOSTAT_H

#include "mqtt_client.h"

void publish_thermostat_data(esp_mqtt_client_handle_t client);
esp_err_t read_thermostat_nvs(const char * tag, int * value);
void update_thermostat(esp_mqtt_client_handle_t client);

struct ThermostatMessage {
  float targetTemperature;
  float targetTemperatureSensibility;
};

void handle_thermostat_cmd_task(void* pvParameters);



#endif /* APP_THERMOSTAT_H */
