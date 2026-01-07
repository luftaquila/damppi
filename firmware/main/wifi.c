#include "esp_wifi.h"
#include "lwip/ip4_addr.h"

#include "main.h"

esp_err_t mqtt_init(void);
void dns_server(void *arg);
void http_server(bool ap_mode);

static const char *TAG = "NET";

static EventGroupHandle_t s_wifi_ev = NULL;

char status[128];

void wifi_softap(void) {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
  ESP_ERROR_CHECK(esp_wifi_init(&(wifi_init_config_t)WIFI_INIT_CONFIG_DEFAULT()));
  ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));

  esp_netif_ip_info_t ip;
  IP4_ADDR(&ip.ip, 192, 168, 4, 1);
  IP4_ADDR(&ip.gw, 192, 168, 4, 1);
  IP4_ADDR(&ip.netmask, 255, 255, 255, 0);

  ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip));
  ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));

  wifi_config_t wifi = {
    .ap = {
      .max_connection = 4,
      .authmode = WIFI_AUTH_OPEN,
    },
  };

  snprintf((char *)wifi.ap.ssid, sizeof(wifi.ap.ssid), "%s", hostname);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "SoftAP SSID: %s", wifi.ap.ssid);
  lcd_printf(LV_FONT(30), 0, "Wi-Fi SSID\n%s", wifi.ap.ssid);

  xTaskCreate(dns_server, "dns_server", 4096, NULL, 10, NULL);
  http_server(true);
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    wifi_event_sta_disconnected_t *disc = (wifi_event_sta_disconnected_t *)data;
    ESP_LOGW(TAG, "STA disconnected, reason=%d", disc->reason);
    esp_wifi_connect();
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    xEventGroupSetBits(s_wifi_ev, BIT0);
  }
}

void wifi_sta(const char *ssid, const char *pass) {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  ESP_ERROR_CHECK(esp_wifi_init(&(wifi_init_config_t)WIFI_INIT_CONFIG_DEFAULT()));

  esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  ESP_ERROR_CHECK(esp_netif_set_hostname(sta, hostname));

  wifi_config_t wifi = { 0 };
  snprintf((char *)wifi.sta.ssid, sizeof(wifi.sta.ssid), "%s", ssid);
  snprintf((char *)wifi.sta.password, sizeof(wifi.sta.password), "%s", pass);
  wifi.sta.scan_method        = WIFI_ALL_CHANNEL_SCAN;
  wifi.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

  s_wifi_ev = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
  ESP_ERROR_CHECK(esp_wifi_start());

  xEventGroupWaitBits(s_wifi_ev, BIT0, false, true, portMAX_DELAY);

  esp_netif_ip_info_t ip;

  if (esp_netif_get_ip_info(sta_netif, &ip) == ESP_OK) {
    ESP_LOGI(TAG, "STA IP: " IPSTR, IP2STR(&ip.ip));
    snprintf(status, sizeof(status), "Wi-Fi: %s\nSERVER: %s\nIP: " IPSTR "\n%s", ssid, server, IP2STR(&ip.ip), name);
    lcd_printf(LV_FONT(24), 5 * 1000, "%s", status);
  }

  http_server(false);
  mqtt_init();
}
