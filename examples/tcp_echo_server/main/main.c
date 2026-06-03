#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp8266_tcp_transport.h"
#include "tcpip_adapter.h"

#include "wifi_credentials.h"

#ifndef TCP_ECHO_PORT
#define TCP_ECHO_PORT 7777
#endif

static const char *TAG = "tcp_echo";

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    (void)ctx;

    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TAG, "wifi sta start");
        esp_wifi_connect();
        break;

    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "wifi connected ip=" IPSTR,
                 IP2STR(&event->event_info.got_ip.ip_info.ip));
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "wifi disconnected, retry");
        esp_wifi_connect();
        break;

    default:
        break;
    }

    return ESP_OK;
}

static esp_err_t wifi_init_sta(void)
{
    tcpip_adapter_init();

    esp_err_t err = esp_event_loop_init(wifi_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_event_loop_init failed err=%d", err);
        return err;
    }

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed err=%d", err);
        return err;
    }

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_storage failed err=%d", err);
        return err;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed err=%d", err);
        return err;
    }

    wifi_config_t wifi_cfg;
    memset(&wifi_cfg, 0, sizeof(wifi_cfg));
    memcpy(wifi_cfg.sta.ssid, WIFI_SSID, sizeof(WIFI_SSID));
    memcpy(wifi_cfg.sta.password, WIFI_PASSWORD, sizeof(WIFI_PASSWORD));

    err = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config failed err=%d", err);
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed err=%d", err);
        return err;
    }

    return ESP_OK;
}

static void on_connect(tcp_conn_t *conn)
{
    ESP_LOGI(TAG, "client connected fd=%d remote_port=%u",
             conn->fd,
             (unsigned)conn->remote_port);
}

static void on_data(tcp_conn_t *conn, const uint8_t *buf, size_t len)
{
    size_t accepted = tcp_send(conn, buf, len);
    if (accepted < len) {
        ESP_LOGI(TAG, "echo truncated fd=%d accepted=%u len=%u",
                 conn->fd,
                 (unsigned)accepted,
                 (unsigned)len);
    }
}

static void on_close(tcp_conn_t *conn, tcp_close_reason_t reason)
{
    ESP_LOGI(TAG, "client closed reason=%d last_error=%d",
             (int)reason,
             conn->last_error);
}

static void on_error(tcp_conn_t *conn, int err)
{
    ESP_LOGE(TAG, "client error fd=%d err=%d", conn->fd, err);
}

void app_main(void)
{
    esp_err_t wifi_ret = wifi_init_sta();
    if (wifi_ret != ESP_OK) {
        ESP_LOGE(TAG, "wifi_init_sta failed err=%d", wifi_ret);
        return;
    }

    tcp_server_callbacks_t callbacks = {
        .on_connect = on_connect,
        .on_data = on_data,
        .on_close = on_close,
        .on_error = on_error,
    };

    int ret = tcp_server_start((uint16_t)TCP_ECHO_PORT,
                               TCP_SERVER_MAX_CLIENTS,
                               &callbacks);
    if (ret != TCP_TRANSPORT_OK) {
        ESP_LOGE(TAG, "tcp_server_start failed ret=%d", ret);
        return;
    }

    ESP_LOGI(TAG, "listening on port %u", (unsigned)TCP_ECHO_PORT);
}
