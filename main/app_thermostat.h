#ifndef APP_THERMOSTAT_H
#define APP_THERMOSTAT_H

#include "mqtt_client.h"

void publish_thermostat_data(esp_mqtt_client_handle_t client);
esp_err_t read_thermostat_nvs(void);
void update_thermostat(void);

struct ThermostatMessage {
  float targetTemperature;
};

void handle_thermostat_cmd_task(void* pvParameters);



#endif /* APP_THERMOSTAT_H */
