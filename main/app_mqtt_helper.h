#ifndef APP_MQTT_HELPER_H
#define APP_MQTT_HELPER_H

void mqtt_helper(void* pvParameters);

#define MAX_SUB 2
#define RELAY_TOPIC CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/cmd/relay"
#define OTA_TOPIC CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/cmd/ota"

#endif /* APP_MQTT_HELPER_H */
