#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "dht.h"

#include "cJSON.h"

#include "esp_http_client.h"
#include "esp_https_ota.h"

static const char *TAG = "MQTTS_EXAMPLE";

static EventGroupHandle_t wifi_event_group;
static EventGroupHandle_t mqtt_event_group;

const static int CONNECTED_BIT = BIT0;
const static int SUBSCRIBED_BIT = BIT1;

static int16_t temperature = 0;
static int16_t humidity = 0;


#define MAX_RELAYS 4

const int ON = 0;
const int OFF = 1;
static int relayStatus[MAX_RELAYS];
static int relayBase = 16;
int relaysNb = 4;



void relays_init()
{
  for(int i = 0; i < relaysNb; i++) {
    gpio_set_direction( relayBase + i, GPIO_MODE_OUTPUT );
    gpio_set_level(relayBase + i, OFF);
    relayStatus[i] = OFF;
  }

}

void publish_relay_data(esp_mqtt_client_handle_t client)
{
  char data[256];
  char relayData[32];
  memset(data,0,256);
  strcat(data, "{\"d\":{");
  for(int i = 0; i < relaysNb; i++) {
    sprintf(relayData, "\"relay%dState\":%d", i, relayStatus[i] == ON);
    if (i != (relaysNb-1)) {
      strcat(relayData, ",");
    }
    strcat(data, relayData);
  }
  strcat(data, "}}");
  int msg_id = esp_mqtt_client_publish(client, "iot-2/evt/relay_status/fmt/json", data,strlen(data), 0, 0);
  ESP_LOGI(TAG, "sent publish relay successful, msg_id=%d", msg_id);

}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
  switch (event->event_id) {
  case SYSTEM_EVENT_STA_START:
    esp_wifi_connect();
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);

    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    esp_wifi_connect();
    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    break;
  default:
    break;
  }
  return ESP_OK;
}

static void wifi_init(void)
{
  tcpip_adapter_init();
  ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  wifi_config_t wifi_config = {
    .sta = {
      .ssid = CONFIG_WIFI_SSID,
      .password = CONFIG_WIFI_PASSWORD,
    },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
  ESP_LOGI(TAG, "start the WIFI SSID:[%s]", CONFIG_WIFI_SSID);
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "Waiting for wifi");
  xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
}

extern const uint8_t messaging_internetofthings_ibmcloud_com_pem_start[] asm("_binary_messaging_internetofthings_ibmcloud_com_pem_start");
extern const uint8_t messaging_internetofthings_ibmcloud_com_pem_end[]   asm("_binary_messaging_internetofthings_ibmcloud_com_pem_end");

int handle_relay_cmd(esp_mqtt_event_handle_t event)
{
  esp_mqtt_client_handle_t client = event->client;
  if (event->data_len >= 32 )
    {
      ESP_LOGI(TAG, "unextected relay cmd payload");
      return -1;
    }
  char tmpBuf[32];
  memcpy(tmpBuf, event->data, event->data_len);
  tmpBuf[event->data_len] = 0;
  cJSON * root   = cJSON_Parse(tmpBuf);
  int id = cJSON_GetObjectItem(root,"id")->valueint;
  int value = cJSON_GetObjectItem(root,"value")->valueint;
  printf("id: %d\r\n", id);
  printf("value: %d\r\n", value);


  if (value == relayStatus[id]) {
    //reversed logic
    if (value == OFF) {
      relayStatus[id] = ON;
    }
    if (value == ON) {
      relayStatus[id] = OFF;
    }
    gpio_set_level(relayBase + id, relayStatus[id]);
    publish_relay_data(client);
  }
  return 0;
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}


int handle_ota_update_cmd(esp_mqtt_event_handle_t event)
{
  esp_mqtt_client_handle_t client = event->client;
  if (event->data_len >= 128 )
    {
      ESP_LOGI(TAG, "unextected relay cmd payload");
      return -1;
    }
  char tmpBuf[128];
  memcpy(tmpBuf, event->data, event->data_len);
  tmpBuf[event->data_len] = 0;
  cJSON * root   = cJSON_Parse(tmpBuf);
  char * url = cJSON_GetObjectItem(root,"ota_url")->valuestring;
  /* int id = cJSON_GetObjectItem(root,"id")->valueint; */
  /* int value = cJSON_GetObjectItem(root,"value")->valueint; */
  /* printf("id: %d\r\n", id); */
  printf("url: %s\r\n", url);
  static const char *TAG = "simple_ota_example";

  ESP_LOGI(TAG, "Starting OTA example...");
  extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
  extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

  esp_http_client_config_t config = {
    .url = url,
    .cert_pem = (char *)server_cert_pem_start,
    .event_handler = _http_event_handler,
  };
  esp_err_t ret = esp_https_ota(&config);
  if (ret == ESP_OK) {
    ESP_LOGE(TAG, "Firmware Upgrades Success, will restart in 10 seconds");
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    esp_restart();
  } else {
    ESP_LOGE(TAG, "Firmware Upgrades Failed");
  }
  return true;
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
  esp_mqtt_client_handle_t client = event->client;
  int msg_id;
  // your_context_t *context = event->context;
  switch (event->event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    xEventGroupSetBits(mqtt_event_group, CONNECTED_BIT);
    msg_id = esp_mqtt_client_subscribe(client, "iotdm-1/mgmt/initiate/device/reboot", 0);
    ESP_LOGI(TAG, "sent subscribe reboot successful, msg_id=%d", msg_id);

    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
    xEventGroupClearBits(mqtt_event_group, CONNECTED_BIT | SUBSCRIBED_BIT);
    break;

  case MQTT_EVENT_SUBSCRIBED:
    if (! (SUBSCRIBED_BIT & xEventGroupGetBits(mqtt_event_group))) {
        msg_id = esp_mqtt_client_subscribe(client, "iot-2/cmd/relay/fmt/json", 0);
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "iot-2/cmd/ota_update/fmt/json", 0);
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);

        xEventGroupSetBits(mqtt_event_group, SUBSCRIBED_BIT);
    }
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_PUBLISHED:
    ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_DATA:
    ESP_LOGI(TAG, "MQTT_EVENT_DATA");
    printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
    printf("DATA=%.*s\r\n", event->data_len, event->data);
    if (strncmp(event->topic, "iot-2/cmd/relay/fmt/json", strlen("iot-2/cmd/relay/fmt/json")) == 0) {
      if (handle_relay_cmd(event)) {
          ESP_LOGI(TAG, "cannot handle relay cmd");
        }
    }

    if (strncmp(event->topic, "iot-2/cmd/ota_update/fmt/json", strlen("iot-2/cmd/ota_update/fmt/json")) == 0) {
      if (handle_ota_update_cmd(event)) {
          ESP_LOGI(TAG, "cannot handle ota_update cmd");
        }
    }
break;
  case MQTT_EVENT_ERROR:
    ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
    break;
  }
  return ESP_OK;
}

static void mqtt_app_start(esp_mqtt_client_handle_t client)
{
  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  esp_mqtt_client_start(client);
}
static const dht_sensor_type_t sensor_type = DHT_TYPE_DHT22;
static const gpio_num_t dht_gpio = 25;

const esp_mqtt_client_config_t mqtt_cfg = {
  .uri = "mqtts://" CONFIG_MQTT_USERNAME ":" CONFIG_MQTT_PASSWORD "@" CONFIG_MQTT_SERVER,
  .event_handle = mqtt_event_handler,
  .cert_pem = (const char *)messaging_internetofthings_ibmcloud_com_pem_start,
  .client_id = CONFIG_MQTT_CLIENT_ID
};

void dht_test(void* pvParameters)
{
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;


  xEventGroupWaitBits(mqtt_event_group, SUBSCRIBED_BIT, false, true, portMAX_DELAY);

  int msg_id;
  while (1)
    {
      if (dht_read_data(sensor_type, dht_gpio, &humidity, &temperature) == ESP_OK)
        {
          printf("Humidity: %.1f%% Temp: %.1fC\n", humidity / 10., temperature / 10.);

          if(true)//FIXME add check to see if connected
            {
              char data[256];
              memset(data,0,256);
              sprintf(data, "{\"d\":{\"counter\":%lld, \"humidity\":%.1f, \"temperature\":%.1f}}",esp_timer_get_time(),humidity / 10., temperature / 10.);
              msg_id = esp_mqtt_client_publish(client, "iot-2/evt/status/fmt/json", data,strlen(data), 0, 0);
              ESP_LOGI(TAG, "sent publish temp successful, msg_id=%d", msg_id);
            }
        }
      else
        printf("Could not read data from sensor\n");

      vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
}


#define BLINK_GPIO 27
void blink_task(void *pvParameter)
{
  /* Configure the IOMUX register for pad BLINK_GPIO (some pads are
     muxed to GPIO on reset already, but some default to other
     functions and need to be switched to GPIO. Consult the
     Technical Reference for a list of pads and their default
     functions.)
  */

  gpio_pad_select_gpio(BLINK_GPIO);
  /* Set the GPIO as a push/pull output */
  gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
  while(1) {
    xEventGroupWaitBits(mqtt_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    gpio_set_level(BLINK_GPIO, ON);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(BLINK_GPIO, OFF);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}


void app_main()
{
  ESP_LOGI(TAG, "[APP] Startup..");
  ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
  esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
  esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

  wifi_event_group = xEventGroupCreate();
  mqtt_event_group = xEventGroupCreate();

  
  xTaskCreate(blink_task, "blink_task", configMINIMAL_STACK_SIZE, NULL, 5, NULL);

  relays_init();

  nvs_flash_init();
  wifi_init();
  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
  mqtt_app_start(client);
  xEventGroupWaitBits(mqtt_event_group, SUBSCRIBED_BIT, false, true, portMAX_DELAY);

  publish_relay_data(client);
  /* xTaskCreate(dht_test, "dht_test", configMINIMAL_STACK_SIZE * 3, (void *)client, 5, NULL); */

}
