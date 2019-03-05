#ifndef APP_RELAY_H
#define APP_RELAY_H

#include "mqtt_client.h"
struct RelayMessage
{
    char relayId;
    char relayValue;
};


void relays_init(void);
void publish_relay_data(esp_mqtt_client_handle_t client);
void handle_relay_cmd_task(void* pvParameters);

#endif /* APP_RELAY_H */
