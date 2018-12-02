#ifndef APP_RELAY_H
#define APP_RELAY_H

#include "mqtt_client.h"

void relays_init(void);
void publish_relay_data(esp_mqtt_client_handle_t client);
int handle_relay_cmd(esp_mqtt_event_handle_t event);

#endif /* APP_RELAY_H */
