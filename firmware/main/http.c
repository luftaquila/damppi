#include "esp_log.h"
#include "esp_http_server.h"

#include "nvs_flash.h"
#include "lwip/sockets.h"

extern nvs_handle_t nvs;
extern char ssid[32];
extern char pass[32];
extern char name[32];
extern char server[16];
extern char hostname[16];

static const char *TAG = "HTTP";

#define HTML_PRE                                                         \
  "<!doctype html><html><head><meta charset='utf-8'/>"                   \
  "<meta name='viewport' content='width=device-width,initial-scale=1'/>" \
  "</head><body style='font-family:system-ui,Arial;margin:16px'>"

static const char *HTML_PAGE_TEMPLATE = HTML_PRE
  "<style>"
  "*{box-sizing:border-box}"
  "label{display:block;margin-top:12px;font-weight:600}"
  "input{width:100%%;min-width:0;padding:10px;margin-top:6px;font-size:16px;border:1px solid #ccc;border-radius:10px}"
  "button{margin-top:16px;padding:12px 14px;font-size:16px;width:100%%;border-radius:12px;border:none}"
  ".card{border:1px solid #ddd;border-radius:12px;padding:14px}"
  ".row{display:flex;gap:10px;flex-wrap:wrap}"
  ".row > div{flex:1 1 240px}"
  ".hint{opacity:.75;font-size:13px;margin-top:10px;line-height:1.4}"
  ".danger{background:#ffd8d8}"
  "</style></head><body>"
  "<h2>%s Configuration</h2>"
  "<div class='card'>"
  "<form method='POST' action='/save'>"
  "<div class='row'>"
  "<div><label>Wi-Fi SSID</label><input name='ssid' required maxlength='31' value='%s'/></div>"
  "<div><label>Wi-Fi Password</label><input name='pass' required maxlength='31' value='%s'/></div>"
  "</div>"
  "<div class='row'>"
  "<div><label>Device Name</label><input name='name' required maxlength='31' value='%s'/></div>"
  "<div><label>Server</label><input name='server' required maxlength='15' inputmode='numeric' value='%s'/></div>"
  "</div>"
  "<button type='submit'>Save</button>"
  "</form>"
  "<form method='POST' action='/reset' onsubmit=\"return confirm('Reset all configurations?');\">"
  "<button class='danger' type='submit'>Reset</button>"
  "</form>"
  "</div>"
  "</body></html>";

static const char *HTML_OK   = HTML_PRE "<h2>Success</h2><p>Device will be rebooted shortly</p></body></html>";
static const char *HTML_FAIL = HTML_PRE "<h2>Error</h2><p>Invalid configuration</p></body></html>";

static int hexval(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }

  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }

  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }

  return -1;
}

static void url_decode_inplace(char *s) {
  char *o = s;

  for (char *p = s; *p; p++) {
    if (*p == '+') {
      *o++ = ' ';
    } else if (*p == '%' && p[1] && p[2]) {
      int a = hexval(p[1]), b = hexval(p[2]);

      if (a >= 0 && b >= 0) {
        *o++ = (char)((a << 4) | b);
        p += 2;
      } else {
        *o++ = *p;
      }
    } else {
      *o++ = *p;
    }
  }
  *o = 0;
}

static esp_err_t send_html(httpd_req_t *req, const char *html) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

esp_err_t root_get(httpd_req_t *req) {
  char out[__builtin_strlen(HTML_PAGE_TEMPLATE) + 120];
  snprintf(out, sizeof(out), HTML_PAGE_TEMPLATE, hostname, ssid, pass, name, server);
  send_html(req, out);
  return ESP_OK;
}

esp_err_t save_post(httpd_req_t *req) {
  int total    = req->content_len;
  int received = 0;

  if (total <= 0 || total > 2048) {
    httpd_resp_set_status(req, "400 Bad Request");
    return send_html(req, HTML_FAIL);
  }

  char *body = (char *)calloc(1, total + 1);

  if (!body) {
    return httpd_resp_send(req, "OOM", HTTPD_RESP_USE_STRLEN);
  }

  while (received < total) {
    int r = httpd_req_recv(req, body + received, total - received);

    if (r <= 0) {
      free(body);
      httpd_resp_set_status(req, "400 Bad Request");
      return send_html(req, HTML_FAIL);
    }

    received += r;
  }

  body[total] = '\0';
  ESP_LOGI(TAG, "POST body: %s", body);

  char new_ssid[32]   = { 0 };
  char new_pass[32]   = { 0 };
  char new_name[32]   = { 0 };
  char new_server[16] = { 0 };
  esp_err_t err       = ESP_OK;

  err |= httpd_query_key_value(body, "ssid", new_ssid, sizeof(new_ssid));
  err |= httpd_query_key_value(body, "pass", new_pass, sizeof(new_pass));
  err |= httpd_query_key_value(body, "name", new_name, sizeof(new_name));
  err |= httpd_query_key_value(body, "server", new_server, sizeof(new_server));

  url_decode_inplace(new_ssid);
  url_decode_inplace(new_pass);
  url_decode_inplace(new_name);
  url_decode_inplace(new_server);

  free(body);

  if (err != ESP_OK || !new_ssid[0] || !new_pass[0] || !new_name[0] || !new_server[0] ||
      inet_pton(AF_INET, new_server, NULL) != 1) {
    httpd_resp_set_status(req, "400 Bad Request");
    return send_html(req, HTML_FAIL);
  }

  ESP_ERROR_CHECK(nvs_set_str(nvs, "ssid", new_ssid));
  ESP_ERROR_CHECK(nvs_set_str(nvs, "pass", new_pass));
  ESP_ERROR_CHECK(nvs_set_str(nvs, "name", new_name));
  ESP_ERROR_CHECK(nvs_set_str(nvs, "server", new_server));
  ESP_ERROR_CHECK(nvs_commit(nvs));

  send_html(req, HTML_OK);
  nvs_close(nvs);
  vTaskDelay(pdMS_TO_TICKS(200));
  esp_restart();
  return ESP_OK;
}

esp_err_t reset_post(httpd_req_t *req) {
  send_html(req, HTML_OK);
  ESP_LOGW(TAG, "Erasing NVS");

  ESP_ERROR_CHECK(nvs_erase_all(nvs));
  ESP_ERROR_CHECK(nvs_commit(nvs));
  nvs_close(nvs);
  vTaskDelay(pdMS_TO_TICKS(200));
  esp_restart();
  return ESP_OK;
}

esp_err_t redirect_root(httpd_req_t *req) {
  httpd_resp_set_status(req, "302 Found");
  httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
  return httpd_resp_send(req, NULL, 0);
}
