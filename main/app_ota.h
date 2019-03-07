#ifndef APP_OTA_H
#define APP_OTA_H

#include "mqtt_client.h"

void handle_ota_update_task(void* pvParameters);

struct OtaMessage
{
  char url[64];
};

#endif /* APP_OTA_H */
