#ifndef MAIN_H
#define MAIN_H

#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "nvs_flash.h"

#define LV_FONT(size) (&lv_font_montserrat_##size)

extern char ssid[32];
extern char pass[32];
extern char name[32];
extern char server[16];
extern char hostname[16];

extern char status[128];

void mqtt_publish(void);
void lcd_printf(const lv_font_t *font, int timeout, const char *fmt, ...);

#endif // MAIN_H
