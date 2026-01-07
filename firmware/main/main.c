#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "lwip/sockets.h"

#include "main.h"

#define BTN_DEBOUNCE_MS 50
#define BTN_MIN_GAP_MS 200
#define BTN_DBL_CLK_MS 500

esp_err_t lcd_init(void);

void wifi_softap(void);
void wifi_sta(const char *ssid, const char *pass);

nvs_handle_t nvs;
char ssid[32];
char pass[32];
char name[32];
char server[16];
char hostname[16];

static const char *TAG = "APP";

static TaskHandle_t btn_task;
static TaskHandle_t reset_task;

static void IRAM_ATTR btn_isr(void *arg) {
  static int64_t last_isr_time   = 0;
  static int64_t last_click_time = 0;

  int64_t now = esp_timer_get_time();

  if (now - last_isr_time < BTN_DEBOUNCE_MS * 1000) {
    return;
  }

  last_isr_time = now;

  if (gpio_get_level(GPIO_NUM_23)) {
    return;
  }

  int64_t diff = now - last_click_time;

  if (diff < BTN_DBL_CLK_MS * 1000) {
    if (diff > BTN_MIN_GAP_MS * 1000) {
      last_click_time = 0;
      xTaskNotifyFromISR(btn_task, 2, eSetBits, NULL);
    }
  } else {
    last_click_time = now;
    xTaskNotifyFromISR(btn_task, 1, eSetBits, NULL);
  }

  portYIELD_FROM_ISR();
}

static void btn_handler(void *arg) {
  uint32_t val;

  while (true) {
    if (xTaskNotifyWait(0, ULONG_MAX, &val, portMAX_DELAY)) {
      if (val & 1) {
        lcd_printf(LV_FONT(30), 3 * 1000, "");
      }

      if (val & 2) {
        mqtt_publish();
      }
    }
  }
}

static void btn_init(void) {
  gpio_config_t gpio = {
    .pin_bit_mask = (1ULL << GPIO_NUM_23),
    .mode         = GPIO_MODE_INPUT,
    .pull_up_en   = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type    = GPIO_INTR_NEGEDGE,
  };

  ESP_ERROR_CHECK(gpio_config(&gpio));
  ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_NUM_23, btn_isr, NULL));

  xTaskCreate(btn_handler, "btn", 2048, NULL, 3, &btn_task);
}

static void reset_isr(void *arg) {
  gpio_num_t pin      = (gpio_num_t)arg;
  static int64_t last = 0;

  if (!gpio_get_level(pin)) {
    last = esp_timer_get_time();
  } else if (esp_timer_get_time() - last > (3 * 1000 * 1000)) {
    vTaskNotifyGiveFromISR(reset_task, NULL);
  }

  portYIELD_FROM_ISR();
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

static void reset_init(void) {
  gpio_config_t gpio;

  gpio.pin_bit_mask = (1ULL << GPIO_NUM_9);
  gpio.mode         = GPIO_MODE_INPUT;
  gpio.pull_up_en   = GPIO_PULLUP_ENABLE;
  gpio.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio.intr_type    = GPIO_INTR_ANYEDGE;

  ESP_ERROR_CHECK(gpio_config(&gpio));
  ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_NUM_9, reset_isr, (void *)GPIO_NUM_9));

  xTaskCreate(reset_handler, "reset", 2048, NULL, 10, &reset_task);
}

void app_main(void) {
  lcd_init();

  ESP_ERROR_CHECK(gpio_install_isr_service(0));
  reset_init();

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
    btn_init();
    wifi_sta(ssid, pass);
  }

  return;
}
