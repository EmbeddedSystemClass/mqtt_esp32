# ESP-MQTT SSL + dh22 + ds sensors + relay control

## MQTT interface:

mqtt_init:
CONNECT:
    .uri = "mqtts://" CONFIG_MQTT_USERNAME ":" CONFIG_MQTT_PASSWORD "@" CONFIG_MQTT_SERVER,
    .cert_pem = (const char *)mqtt_iot_cipex_ro_pem_start,
    .client_id = CONFIG_MQTT_CLIENT_ID,
    .lwt_topic = CONFIG_MQTT_DEVICE_TYPE"/"CONFIG_MQTT_CLIENT_ID"/evt/disconnected",
    .keepalive = 30

wait_for(CONNECTED_BIT):

subscribe()//blocking with flags for subscriptions
publish()//blocking with flags waiting for publish
  connect + fw version + default data
  relays
wait_for():

on MQTT_EVENT_CONNECTED:
    xEventGroupSetBits(mqtt_event_group, CONNECTED_BIT | READY_FOR_REQUEST);
    

## threads model:
 main
 mqtt_handler (build-in)
 sensors (loop sonsor read and mqtt publish)
 blink - to be completed
 relays(to_be_created) - queue read from mqttt_handler
 ota(to_be_created) - queue read from mqtt_handler (to be implemented)

## todo
 * add ota to dedicated thread (check if ota stability is improved)
 * remove unused code/flags/etc.
 * add variable read from nv and publish in connect message (temp + sensibility)
 * subscribe to variable change and save to nvram
 * rewrite relays from esp8266
 * implement/check thermostat code

 * sonoff basic
   * glue serial
   * connect and monitor by serial
   * write 8266 firmware
   * check ota
   * connect jack socket for sensor
 * sonoff switch
   * glue serial
   * connect and monitor by serial
   * write 8266 firmware
   * check ota
   * design implement software for control