#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_system.h"

#include "nvs_flash.h"
#include "driver/gpio.h"
#include "lwip/sockets.h"

void wifi_softap(void);
void wifi_sta(const char *ssid, const char *pass);

nvs_handle_t nvs;
char ssid[32];
char pass[32];
char name[32];
char server[16];
char hostname[16];

static const char *TAG = "APP";

static TaskHandle_t reset_task;

static void reset_isr(void *arg) {
  gpio_num_t pin       = (gpio_num_t)arg;
  static int64_t press = 0;

  if (!gpio_get_level(pin)) {
    press = esp_timer_get_time();
  } else if (esp_timer_get_time() - press > (3 * 1000 * 1000)) {
    vTaskNotifyGiveFromISR(reset_task, NULL);
  }
}

static void reset_handler(void *arg) {
  while (true) {
    ulTaskNotifyTake(true, portMAX_DELAY);

    ESP_LOGW(TAG, "Erasing NVS");
    ESP_ERROR_CHECK(nvs_erase_all(nvs));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
    esp_restart();
  }
}

static void init_reset(void) {
  const gpio_num_t pin = GPIO_NUM_9;
  gpio_config_t gpio;

  gpio.pin_bit_mask = (1ULL << pin);
  gpio.mode         = GPIO_MODE_INPUT;
  gpio.pull_up_en   = GPIO_PULLUP_ENABLE;
  gpio.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio.intr_type    = GPIO_INTR_ANYEDGE;

  ESP_ERROR_CHECK(gpio_config(&gpio));
  ESP_ERROR_CHECK(gpio_install_isr_service(0));
  ESP_ERROR_CHECK(gpio_isr_handler_add(pin, reset_isr, (void *)pin));

  xTaskCreate(reset_handler, "reset", 2048, NULL, 10, &reset_task);
}

void app_main(void) {
  init_reset();

  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(nvs_open("cfg", NVS_READWRITE, &nvs));

  esp_err_t err = ESP_OK;

  size_t size = sizeof(ssid);
  err |= nvs_get_str(nvs, "ssid", ssid, &size);

  size = sizeof(pass);
  err |= nvs_get_str(nvs, "pass", pass, &size);

  size = sizeof(name);
  err |= nvs_get_str(nvs, "name", name, &size);

  size = sizeof(server);
  err |= nvs_get_str(nvs, "server", server, &size);

  uint8_t mac[6];
  ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
  snprintf(hostname, sizeof(hostname), "Damppi %02X%02X%02X", mac[3], mac[4], mac[5]);

  if (err != ESP_OK || !ssid[0] || !pass[0] || !name[0] || !server[0] || inet_pton(AF_INET, server, NULL) != 1) {
    wifi_softap();
  } else {
    wifi_sta(ssid, pass);
  }

  return;
}
