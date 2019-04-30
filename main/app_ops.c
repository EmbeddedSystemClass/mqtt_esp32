#include "esp_system.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "app_esp32.h"
#include "app_ops.h"

static const char *TAG = "MQTTS_OPS";

extern EventGroupHandle_t mqtt_event_group;
extern const int MQTT_PUBLISHED_BIT;
extern const int MQTT_INIT_FINISHED_BIT;


void vTaskGetRunTimeStatsAsJson( char *pcWriteBuffer )
{
  TaskStatus_t *pxTaskStatusArray;
  volatile UBaseType_t uxArraySize, x;
  uint32_t ulTotalTime, ulStatsAsPercentage;

#if( configUSE_TRACE_FACILITY != 1 )
  {
#error configUSE_TRACE_FACILITY must also be set to 1 in FreeRTOSConfig.h to use vTaskGetRunTimeStats().
  }
#endif

  /*
   * PLEASE NOTE:
   *
   * This function is provided for convenience only, and is used by many
   * of the demo applications.  Do not consider it to be part of the
   * scheduler.
   *
   * vTaskGetRunTimeStats() calls uxTaskGetSystemState(), then formats part
   * of the uxTaskGetSystemState() output into a human readable table that
   * displays the amount of time each task has spent in the Running state
   * in both absolute and percentage terms.
   *
   * vTaskGetRunTimeStats() has a dependency on the sprintf() C library
   * function that might bloat the code size, use a lot of stack, and
   * provide different results on different platforms.  An alternative,
   * tiny, third party, and limited functionality implementation of
   * sprintf() is provided in many of the FreeRTOS/Demo sub-directories in
   * a file called printf-stdarg.c (note printf-stdarg.c does not provide
   * a full snprintf() implementation!).
   *
   * It is recommended that production systems call uxTaskGetSystemState()
   * directly to get access to raw stats data, rather than indirectly
   * through a call to vTaskGetRunTimeStats().
   */

  /* Make sure the write buffer does not contain a string. */
  *pcWriteBuffer = 0x00;

  /* Take a snapshot of the number of tasks in case it changes while this
     function is executing. */
  uxArraySize = uxTaskGetNumberOfTasks();

  /* Allocate an array index for each task.  NOTE!  If
     configSUPPORT_DYNAMIC_ALLOCATION is set to 0 then pvPortMalloc() will
     equate to NULL. */
  pxTaskStatusArray = pvPortMalloc( uxArraySize * sizeof( TaskStatus_t ) );

  if( pxTaskStatusArray != NULL )
    {
      /* Generate the (binary) data. */
      uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalTime );

      /* For percentage calculations. */
      ulTotalTime /= 100UL;

      /* Avoid divide by zero errors. */
      if( ulTotalTime > 0 )
        {
          sprintf(pcWriteBuffer, "{");
          pcWriteBuffer += strlen( pcWriteBuffer );
          /* Create a human readable table from the binary data. */
          for( x = 0; x < uxArraySize; x++ )
            {
              /* What percentage of the total run time has the task used?
                 This will always be rounded down to the nearest integer.
                 ulTotalRunTimeDiv100 has already been divided by 100. */
              ulStatsAsPercentage = pxTaskStatusArray[ x ].ulRunTimeCounter / ulTotalTime;
              sprintf(pcWriteBuffer, "\"%s\":", pxTaskStatusArray[ x ].pcTaskName);
              pcWriteBuffer += strlen( pcWriteBuffer );
              sprintf( pcWriteBuffer, "%d,", ulStatsAsPercentage );
              pcWriteBuffer += strlen( pcWriteBuffer );

            }
          sprintf(pcWriteBuffer-1, "}");

        }
    }
  vPortFree( pxTaskStatusArray );
}


void publish_ops_data(esp_mqtt_client_handle_t client)
{
  if (xEventGroupGetBits(mqtt_event_group) & MQTT_INIT_FINISHED_BIT)
    {
      const char * connect_topic = CONFIG_MQTT_DEVICE_TYPE "/" CONFIG_MQTT_CLIENT_ID "/evt/ops";
      char data[512];
      memset(data,0,512);

      char tasks_info[512-64];
      memset(tasks_info,0,512-64);
      vTaskGetRunTimeStatsAsJson(tasks_info );

      sprintf(data, "{\"free_heap\":%d, \"min_free_heap\":%d, \"tasks_stats\":%s}",
              esp_get_free_heap_size(),
              esp_get_minimum_free_heap_size(),
              tasks_info
              );
      xEventGroupClearBits(mqtt_event_group, MQTT_PUBLISHED_BIT);
      int msg_id = esp_mqtt_client_publish(client, connect_topic, data,strlen(data), 1, 0);
      if (msg_id > 0) {
        ESP_LOGI(TAG, "sent publish connected data successful, msg_id=%d", msg_id);
        EventBits_t bits = xEventGroupWaitBits(mqtt_event_group, MQTT_PUBLISHED_BIT, false, true, MQTT_FLAG_TIMEOUT);
        if (bits & MQTT_PUBLISHED_BIT) {
          ESP_LOGI(TAG, "publish ack received, msg_id=%d", msg_id);
        } else {
          ESP_LOGW(TAG, "publish ack not received, msg_id=%d", msg_id);
        }
      } else {
        ESP_LOGW(TAG, "failed to publish connected data, msg_id=%d", msg_id);
      }
    }
}


void ops_pub_task(void* pvParameters)
{
  esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t) pvParameters;
  while (1) {
    publish_ops_data(client);
    vTaskDelay(60000 / portTICK_PERIOD_MS);
  }
}

