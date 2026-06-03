#include <stddef.h>
#include <stdint.h>

#include "esp_log.h"
#include "tcp_transport.h"

#ifndef TCP_ECHO_PORT
#define TCP_ECHO_PORT 7777
#endif

static const char *TAG = "tcp_echo";

static void on_connect(tcp_conn_t *conn)
{
    ESP_LOGI(TAG, "client connected fd=%d", conn->fd);
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

static void on_close(tcp_conn_t *conn)
{
    ESP_LOGI(TAG, "client closed state=%d", (int)conn->state);
}

static void on_error(tcp_conn_t *conn, int err)
{
    ESP_LOGE(TAG, "client error fd=%d err=%d", conn->fd, err);
}

void app_main(void)
{
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
