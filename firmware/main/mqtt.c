#include "esp_err.h"
#include "mqtt_client.h"

#include "main.h"

#define MQTT_CHANNEL "channel/0"

esp_mqtt_client_handle_t mqtt;

static const char *TAG = "MQTT";

void mqtt_publish(void) {
  if (mqtt) {
    esp_mqtt_client_publish(mqtt, MQTT_CHANNEL, name, strlen(name), 1, false);
    ESP_LOGI(TAG, "Published to topic %s: %s", MQTT_CHANNEL, name);
  } else {
    ESP_LOGW(TAG, "client not initialized");
    lcd_printf(LV_FONT(24), 10 * 1000, "MQTT\nnot initialized");
  }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = event_data;

  switch (event_id) {
    case MQTT_EVENT_CONNECTED:
      esp_mqtt_client_subscribe(mqtt, MQTT_CHANNEL, 1);
      ESP_LOGI(TAG, "connected");
      break;
    case MQTT_EVENT_DISCONNECTED:
      ESP_LOGW(TAG, "disconnected");
      break;
    case MQTT_EVENT_DATA:
      ESP_LOGI( TAG, "data received on topic %.*s: %.*s", event->topic_len, event->topic, event->data_len, event->data);
      lcd_printf(LV_FONT(30), 60 * 1000, "%.*s\ncalled everyone!", event->data_len, event->data);
      break;
    case MQTT_EVENT_ERROR:
      ESP_LOGW(TAG, "error %d", event->error_handle->error_type);
      lcd_printf(LV_FONT(24), 10 * 1000, "MQTT error %d", event->error_handle->error_type);
      break;
    default:
      break;
  }
}

esp_err_t mqtt_init(void) {
  char mqtt_url[64];
  snprintf(mqtt_url, sizeof(mqtt_url), "mqtt://%s", server);

  esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = mqtt_url,
    .session.keepalive  = 10,
    .task.priority      = 5,
  };

  mqtt = esp_mqtt_client_init(&mqtt_cfg);

  ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
  ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt));

  ESP_LOGI(TAG, "initialized with broker %s", mqtt_url);

  return ESP_OK;
}
