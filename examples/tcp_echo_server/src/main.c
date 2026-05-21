#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_common.h"
#include "tcp_transport.h"

#ifndef WIFI_SSID
#define WIFI_SSID "your-ssid"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "your-password"
#endif

#ifndef TCP_ECHO_PORT
#define TCP_ECHO_PORT 7777
#endif

static bool s_server_started;

uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();

    switch (size_map) {
    case FLASH_SIZE_4M_MAP_256_256:
        return 128 - 5;
    case FLASH_SIZE_8M_MAP_512_512:
        return 256 - 5;
    case FLASH_SIZE_16M_MAP_512_512:
    case FLASH_SIZE_16M_MAP_1024_1024:
        return 512 - 5;
    case FLASH_SIZE_32M_MAP_512_512:
    case FLASH_SIZE_32M_MAP_1024_1024:
        return 1024 - 5;
    case FLASH_SIZE_64M_MAP_1024_1024:
        return 2048 - 5;
    case FLASH_SIZE_128M_MAP_1024_1024:
        return 4096 - 5;
    default:
        return 0;
    }
}

static void on_connect(tcp_conn_t *conn)
{
    os_printf("tcp_echo: client connected fd=%d\n", conn->fd);
}

static void on_data(tcp_conn_t *conn, const uint8_t *buf, size_t len)
{
    size_t accepted = tcp_send(conn, buf, len);
    if (accepted < len) {
        os_printf("tcp_echo: echo truncated fd=%d accepted=%u len=%u\n",
                  conn->fd,
                  (unsigned)accepted,
                  (unsigned)len);
    }
}

static void on_close(tcp_conn_t *conn)
{
    os_printf("tcp_echo: client closed state=%d\n", (int)conn->state);
}

static void on_error(tcp_conn_t *conn, int err)
{
    os_printf("tcp_echo: client error fd=%d err=%d\n", conn->fd, err);
}

static void start_echo_server_once(void)
{
    if (s_server_started) {
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
        os_printf("tcp_echo: tcp_server_start failed ret=%d\n", ret);
        return;
    }

    s_server_started = true;
    os_printf("tcp_echo: listening on port %u\n", (unsigned)TCP_ECHO_PORT);
}

static void wifi_event_handler_cb(System_Event_t *event)
{
    if (event == NULL) {
        return;
    }

    switch (event->event_id) {
    case EVENT_STAMODE_GOT_IP:
        os_printf("tcp_echo: wifi got ip\n");
        start_echo_server_once();
        break;

    case EVENT_STAMODE_DISCONNECTED:
        os_printf("tcp_echo: wifi disconnected\n");
        break;

    default:
        break;
    }
}

void user_init(void)
{
    os_printf("tcp_echo: SDK version %s heap=%u\n",
              system_get_sdk_version(),
              system_get_free_heap_size());

    wifi_set_opmode(STATION_MODE);
    wifi_set_event_handler_cb(wifi_event_handler_cb);

    struct station_config config;
    memset(&config, 0, sizeof(config));
    snprintf((char *)config.ssid, sizeof(config.ssid), "%s", WIFI_SSID);
    snprintf((char *)config.password, sizeof(config.password), "%s", WIFI_PASSWORD);

    wifi_station_set_config(&config);
    wifi_station_connect();
}
