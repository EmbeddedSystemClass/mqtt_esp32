#ifndef APP_SENSORS_H
#define APP_SENSORS_H

void sensors_read(void* pvParameters);
void publish_sensor_data(esp_mqtt_client_handle_t client);

#endif /* APP_SENSORS_H */
