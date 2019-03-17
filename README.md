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


## LED interface:
 * led on after startup, fast-blink after wifi and normal blink after mqtt
 
## todo
 * rewrite ota using native method and check for error 76 or size and retry
 * remove unused code/flags/etc.
 * implement/check thermostat code
 * disable ota while heating??

## esp8266 
 * investigate reconect failure
 * rewrite mqtt handling for most common code between implementations (common topic handlers)
 * test wifi reconnect
 * test ota
 * test /update relay conrol
 * move to sonoff


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