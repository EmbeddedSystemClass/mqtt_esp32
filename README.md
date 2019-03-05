# ESP-MQTT SSL + dh22 + ds sensors + relay control

MQTT interface:

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
wait_for():

on MQTT_EVENT_CONNECTED:
    xEventGroupSetBits(mqtt_event_group, CONNECTED_BIT | READY_FOR_REQUEST);
    

