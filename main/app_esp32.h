#ifndef APP_ESP32_H
#define APP_ESP32_H

#define ON 0
#define OFF 1

#define MQTT_TIMEOUT 30
#define MQTT_FLAG_TIMEOUT (MQTT_TIMEOUT * 1000 / portTICK_PERIOD_MS)
#endif /* APP_ESP32_H */
