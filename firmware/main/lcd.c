#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lvgl_port.h"
#include "driver/gpio.h"

#define LCD_WIDTH 172
#define LCD_HEIGHT 320
#define BACKLIGHT GPIO_NUM_22

#define UI_QUEUE_LEN 4
#define MAX_TEXT_LEN 128

extern const lv_image_dsc_t logo;

static esp_lcd_panel_handle_t lcd = NULL;
static QueueHandle_t ui_queue     = NULL;
static lv_obj_t *ui_label         = NULL;

typedef struct {
  const lv_font_t *font;
  char text[MAX_TEXT_LEN];
} ui_msg_t;

void ui_task(void *arg) {
  ui_msg_t msg;
  TickType_t wait = portMAX_DELAY;
  bool first      = true;

  while (true) {
    if (xQueueReceive(ui_queue, &msg, wait)) {
      gpio_set_level(BACKLIGHT, 1);
      esp_lcd_panel_disp_on_off(lcd, true);

      lvgl_port_lock(0);

      if (first) {
        lv_obj_clean(lv_screen_active());

        ui_label = lv_label_create(lv_screen_active());
        lv_obj_set_style_text_color(ui_label, lv_color_white(), 0);
        lv_obj_set_style_text_align(ui_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(ui_label, "");

        first = false;
      }

      lv_obj_set_style_text_font(ui_label, msg.font, 0);
      lv_label_set_text(ui_label, msg.text);
      lv_obj_center(ui_label);

      lvgl_port_unlock();

      wait = pdMS_TO_TICKS(5000);
    } else {
      esp_lcd_panel_disp_on_off(lcd, false);
      gpio_set_level(BACKLIGHT, 0);
      wait = portMAX_DELAY;
    }
  }
}

void lcd_printf(const lv_font_t *font, const char *fmt, ...) {
  ui_msg_t msg;
  msg.font = font;

  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg.text, MAX_TEXT_LEN, fmt, ap);
  va_end(ap);

  xQueueSend(ui_queue, &msg, pdMS_TO_TICKS(10));
}

esp_err_t lcd_init(void) {
  spi_bus_config_t spi = {
    .mosi_io_num     = GPIO_NUM_6,
    .sclk_io_num     = GPIO_NUM_7,
    .miso_io_num     = -1,
    .quadwp_io_num   = -1,
    .quadhd_io_num   = -1,
    .max_transfer_sz = LCD_WIDTH * 40 * 2 + 8,
  };

  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &spi, SPI_DMA_CH_AUTO));

  esp_lcd_panel_io_handle_t lcd_io            = NULL;
  esp_lcd_panel_io_spi_config_t lcd_io_config = {
    .dc_gpio_num       = GPIO_NUM_15,
    .cs_gpio_num       = GPIO_NUM_14,
    .pclk_hz           = 40 * 1000 * 1000,
    .lcd_cmd_bits      = 8,
    .lcd_param_bits    = 8,
    .spi_mode          = 0,
    .trans_queue_depth = 8,
  };

  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &lcd_io_config, &lcd_io));

  esp_lcd_panel_dev_config_t lcd_cfg = {
    .reset_gpio_num = GPIO_NUM_21,
    .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
    .bits_per_pixel = 16,
  };

  ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(lcd_io, &lcd_cfg, &lcd));

  gpio_config_t gpio = {
    .pin_bit_mask = 1ULL << BACKLIGHT,
    .mode         = GPIO_MODE_OUTPUT,
    .pull_up_en   = 0,
    .pull_down_en = 0,
    .intr_type    = GPIO_INTR_DISABLE,
  };
  gpio_config(&gpio);

  ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd));
  ESP_ERROR_CHECK(esp_lcd_panel_init(lcd));

  gpio_set_level(BACKLIGHT, true);
  esp_lcd_panel_set_gap(lcd, 0, 34);
  esp_lcd_panel_invert_color(lcd, true);
  esp_lcd_panel_disp_on_off(lcd, true);

  const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
  ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

  const lvgl_port_display_cfg_t disp_cfg = {
    .io_handle     = lcd_io,
    .panel_handle  = lcd,
    .buffer_size   = LCD_HEIGHT * 40,
    .double_buffer = true,
    .hres          = LCD_HEIGHT,
    .vres          = LCD_WIDTH,
    .monochrome    = false,
    .color_format  = LV_COLOR_FORMAT_RGB565,
    .rotation = {
      .swap_xy  = true,
      .mirror_x = true,
      .mirror_y = false,
    },
    .flags = {
      .swap_bytes = true,
    },
  };

  lvgl_port_add_disp(&disp_cfg);

  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
  lv_obj_t *img = lv_image_create(lv_screen_active());
  lv_image_set_src(img, &logo);
  lv_obj_center(img);

  ui_queue = xQueueCreate(UI_QUEUE_LEN, sizeof(ui_msg_t));
  xTaskCreate(ui_task, "ui", 4096, NULL, 5, NULL);

  return ESP_OK;
}
