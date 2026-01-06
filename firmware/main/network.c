#include "esp_log.h"
#include "esp_http_server.h"

#include "lwip/sockets.h"

esp_err_t root_get(httpd_req_t *req);
esp_err_t save_post(httpd_req_t *req);
esp_err_t reset_post(httpd_req_t *req);
esp_err_t redirect_root(httpd_req_t *req);

static const char *TAG = "SRV";

void dns_server(void *arg) {
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (sock < 0) {
    ESP_LOGE(TAG, "DNS socket create failed");
    vTaskDelete(NULL);
    return;
  }

  struct sockaddr_in addr = { 0 };
  addr.sin_family         = AF_INET;
  addr.sin_port           = htons(53);
  addr.sin_addr.s_addr    = htonl(INADDR_ANY);

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    ESP_LOGE(TAG, "DNS bind failed");
    close(sock);
    vTaskDelete(NULL);
    return;
  }

  uint8_t rx[512];
  uint8_t tx[512];

  while (true) {
    struct sockaddr_in from = { 0 };
    socklen_t fromlen       = sizeof(from);
    int len                 = recvfrom(sock, rx, sizeof(rx), 0, (struct sockaddr *)&from, &fromlen);

    if (len < 12 || len > (sizeof(tx) - 16)) {
      continue;
    }

    memcpy(tx, rx, len);
    tx[2] = 0x81;
    tx[3] = 0x80;  // response, no error
    tx[6] = 0x00;
    tx[7] = 0x01;                            // ANCOUNT=1
    tx[8] = tx[9] = tx[10] = tx[11] = 0x00;  // NS/AR=0

    int p   = len;
    tx[p++] = 0xC0;
    tx[p++] = 0x0C;  // NAME ptr
    tx[p++] = 0x00;
    tx[p++] = 0x01;  // TYPE A
    tx[p++] = 0x00;
    tx[p++] = 0x01;  // CLASS IN
    tx[p++] = 0x00;
    tx[p++] = 0x00;
    tx[p++] = 0x00;
    tx[p++] = 0x3C;  // TTL
    tx[p++] = 0x00;
    tx[p++] = 0x04;  // RDLENGTH
    tx[p++] = 192;
    tx[p++] = 168;
    tx[p++] = 4;
    tx[p++] = 1;  // 192.168.4.1

    (void)sendto(sock, tx, p, 0, (struct sockaddr *)&from, fromlen);
  }
}

void http_server(bool ap_mode) {
  httpd_handle_t httpd;
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();

  cfg.max_uri_handlers = 24;
  ESP_ERROR_CHECK(httpd_start(&httpd, &cfg));

  httpd_uri_t u_root  = { .uri = "/", .method = HTTP_GET, .handler = root_get };
  httpd_uri_t u_save  = { .uri = "/save", .method = HTTP_POST, .handler = save_post };
  httpd_uri_t u_reset = { .uri = "/reset", .method = HTTP_POST, .handler = reset_post };
  ESP_ERROR_CHECK(httpd_register_uri_handler(httpd, &u_root));
  ESP_ERROR_CHECK(httpd_register_uri_handler(httpd, &u_save));
  ESP_ERROR_CHECK(httpd_register_uri_handler(httpd, &u_reset));

  if (ap_mode) {
    static const char *captive_paths[] = {
      "/generate_204",
      "/gen_204",
      "/ncsi.txt",
      "/connecttest.txt",
      "/hotspot-detect.html",
      "/library/test/success.html",
      "/success.txt",
      "/favicon.ico",
      "/redirect",
    };

    for (int i = 0; i < sizeof(captive_paths) / sizeof(captive_paths[0]); i++) {
      httpd_uri_t u = { .uri = captive_paths[i], .method = HTTP_GET, .handler = redirect_root };
      httpd_register_uri_handler(httpd, &u);
    }
  }

  ESP_LOGI(TAG, "HTTP server started");
}
