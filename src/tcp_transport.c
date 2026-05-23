#include "tcp_transport.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

/* Stack size in FreeRTOS words. Default keeps room for application callbacks
 * executed from the network task; reduce only after measuring the watermark.
 */
#ifndef TCP_NETWORK_TASK_STACK_SIZE
#define TCP_NETWORK_TASK_STACK_SIZE 1024U
#endif

#ifndef TCP_TRANSPORT_STACK_CHECK
#define TCP_TRANSPORT_STACK_CHECK   0
#endif

#ifndef TCP_NETWORK_TASK_PRIORITY
#define TCP_NETWORK_TASK_PRIORITY   5U
#endif

#ifndef TCP_LISTEN_BACKLOG
#define TCP_LISTEN_BACKLOG          TCP_SERVER_MAX_CLIENTS
#endif

#ifndef TCP_ENABLE_REUSEADDR
#if defined(SO_REUSE) && SO_REUSE
#define TCP_ENABLE_REUSEADDR        1
#else
#define TCP_ENABLE_REUSEADDR        0
#endif
#endif

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) ((portTickType)(((ms) + portTICK_RATE_MS - 1U) / portTICK_RATE_MS))
#endif

#ifndef TCP_TRANSPORT_LOGI
#define TCP_TRANSPORT_LOGI(fmt, ...) printf("[tcp_transport] " fmt "\n", ##__VA_ARGS__)
#endif

#ifndef TCP_TRANSPORT_LOGE
#define TCP_TRANSPORT_LOGE(fmt, ...) printf("[tcp_transport][error] " fmt "\n", ##__VA_ARGS__)
#endif

typedef struct {
    int listen_fd;
    uint16_t port;
    uint8_t max_clients;
    tcp_server_callbacks_t callbacks;
    xTaskHandle task_handle;
    volatile bool running;
    bool started;
    tcp_conn_t slots[TCP_SERVER_MAX_CLIENTS];
} tcp_server_t;

static tcp_server_t s_server = {
    .listen_fd = -1,
};

static uint32_t tcp_now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_RATE_MS);
}

static bool tcp_is_wouldblock(int err)
{
    return err == EAGAIN || err == EWOULDBLOCK;
}

static bool tcp_is_retryable_socket_error(int err)
{
    return tcp_is_wouldblock(err) || err == EINTR;
}

static void tcp_reset_slot(tcp_conn_t *conn)
{
    if (conn == NULL) {
        return;
    }

    conn->fd = -1;
    conn->rx_len = 0;
    conn->tx_len = 0;
    conn->tx_offset = 0;
    conn->last_activity_ms = 0;
    conn->state = TCP_SLOT_FREE;
    conn->close_after_drain = false;
}

static void tcp_reset_all_slots(void)
{
    for (uint8_t i = 0; i < TCP_SERVER_MAX_CLIENTS; ++i) {
        tcp_reset_slot(&s_server.slots[i]);
    }
}

static bool tcp_conn_belongs_to_server(const tcp_conn_t *conn)
{
    if (conn == NULL) {
        return false;
    }

    uintptr_t ptr = (uintptr_t)conn;
    uintptr_t begin = (uintptr_t)&s_server.slots[0];
    uintptr_t end = (uintptr_t)&s_server.slots[TCP_SERVER_MAX_CLIENTS];

    return ptr >= begin
        && ptr < end
        && ((ptr - begin) % sizeof(tcp_conn_t)) == 0U;
}

static bool tcp_is_network_task(void)
{
    return s_server.task_handle != NULL
        && xTaskGetCurrentTaskHandle() == s_server.task_handle;
}

static tcp_conn_t *tcp_find_free_slot(void)
{
    for (uint8_t i = 0; i < s_server.max_clients; ++i) {
        if (s_server.slots[i].state == TCP_SLOT_FREE) {
            return &s_server.slots[i];
        }
    }

    return NULL;
}

static int tcp_set_nonblocking(int fd)
{
    unsigned long nonblocking = 1;

    return ioctlsocket(fd, FIONBIO, &nonblocking);
}

static void tcp_close_fd(int *fd)
{
    if (fd != NULL && *fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

static void tcp_close_slot(tcp_conn_t *conn, bool notify_close)
{
    if (!tcp_conn_belongs_to_server(conn)
        || conn->state == TCP_SLOT_FREE
        || conn->state == TCP_SLOT_CLOSING) {
        return;
    }

    int closed_fd = conn->fd;
    conn->state = TCP_SLOT_CLOSING;
    tcp_close_fd(&conn->fd);
    TCP_TRANSPORT_LOGI("client closed fd=%d", closed_fd);

    if (notify_close && s_server.callbacks.on_close != NULL) {
        s_server.callbacks.on_close(conn);
    }

    tcp_reset_slot(conn);
}

static void tcp_fail_slot(tcp_conn_t *conn, int err)
{
    if (!tcp_conn_belongs_to_server(conn) || conn->state == TCP_SLOT_FREE) {
        return;
    }

    conn->state = TCP_SLOT_ERROR;
    TCP_TRANSPORT_LOGE("client error fd=%d err=%d", conn->fd, err);

    if (s_server.callbacks.on_error != NULL) {
        s_server.callbacks.on_error(conn, err);
    }

    tcp_close_slot(conn, true);
}

static void tcp_mark_tx_empty(tcp_conn_t *conn)
{
    conn->tx_len = 0;
    conn->tx_offset = 0;

    if (conn->close_after_drain) {
        tcp_close_slot(conn, true);
    }
}

static void tcp_handle_accept(void)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(s_server.listen_fd, (struct sockaddr *)&client_addr, &client_len);

    if (client_fd < 0) {
        int err = errno;
        if (!tcp_is_retryable_socket_error(err)) {
            TCP_TRANSPORT_LOGE("accept failed err=%d", err);
        }
        return;
    }

    if (tcp_set_nonblocking(client_fd) < 0) {
        int err = errno;
        TCP_TRANSPORT_LOGE("client nonblock failed fd=%d err=%d", client_fd, err);
        close(client_fd);
        return;
    }

    tcp_conn_t *slot = tcp_find_free_slot();
    if (slot == NULL) {
        TCP_TRANSPORT_LOGI("client rejected: max clients reached");
        close(client_fd);
        return;
    }

    tcp_reset_slot(slot);
    slot->fd = client_fd;
    slot->state = TCP_SLOT_USED;
    slot->last_activity_ms = tcp_now_ms();

    TCP_TRANSPORT_LOGI("client accepted fd=%d", client_fd);

    if (s_server.callbacks.on_connect != NULL) {
        s_server.callbacks.on_connect(slot);
    }
}

static void tcp_handle_rx(tcp_conn_t *conn)
{
    ssize_t ret = recv(conn->fd, conn->rx_buf, sizeof(conn->rx_buf), 0);

    if (ret > 0) {
        conn->rx_len = (size_t)ret;
        conn->last_activity_ms = tcp_now_ms();

        if (s_server.callbacks.on_data != NULL) {
            s_server.callbacks.on_data(conn, conn->rx_buf, conn->rx_len);
        }
        return;
    }

    if (ret == 0) {
        tcp_close_slot(conn, true);
        return;
    }

    int err = errno;
    if (!tcp_is_retryable_socket_error(err)) {
        tcp_fail_slot(conn, err);
    }
}

static void tcp_handle_tx(tcp_conn_t *conn)
{
    if (conn->tx_offset >= conn->tx_len) {
        tcp_mark_tx_empty(conn);
        return;
    }

    const uint8_t *buf = &conn->tx_buf[conn->tx_offset];
    size_t remaining = conn->tx_len - conn->tx_offset;
    ssize_t ret = send(conn->fd, buf, remaining, 0);

    if (ret > 0) {
        conn->tx_offset += (size_t)ret;
        conn->last_activity_ms = tcp_now_ms();

        if (conn->tx_offset == conn->tx_len) {
            tcp_mark_tx_empty(conn);
        }
        return;
    }

    if (ret < 0) {
        int err = errno;
        if (!tcp_is_retryable_socket_error(err)) {
            tcp_fail_slot(conn, err);
        }
    }
}

static void tcp_check_idle_clients(void)
{
#if TCP_IDLE_TIMEOUT_MS > 0
    uint32_t now = tcp_now_ms();

    for (uint8_t i = 0; i < s_server.max_clients; ++i) {
        tcp_conn_t *conn = &s_server.slots[i];
        if (conn->state != TCP_SLOT_USED) {
            continue;
        }

        if ((uint32_t)(now - conn->last_activity_ms) >= TCP_IDLE_TIMEOUT_MS) {
            TCP_TRANSPORT_LOGI("client idle timeout fd=%d", conn->fd);
            tcp_close_slot(conn, true);
        }
    }
#endif
}

static void tcp_close_all_clients(void)
{
    for (uint8_t i = 0; i < TCP_SERVER_MAX_CLIENTS; ++i) {
        tcp_close_slot(&s_server.slots[i], true);
    }
}

static void tcp_network_task(void *arg)
{
    (void)arg;
#if TCP_TRANSPORT_STACK_CHECK
    bool stack_warning_logged = false;
#endif

    while (s_server.running) {
#if TCP_TRANSPORT_STACK_CHECK
        unsigned portBASE_TYPE watermark = uxTaskGetStackHighWaterMark(NULL);
        if (!stack_warning_logged && watermark < 64U) {
            TCP_TRANSPORT_LOGE("stack watermark low: %u words", (unsigned)watermark);
            stack_warning_logged = true;
        }
#endif

        fd_set rfds;
        fd_set wfds;
        int max_fd = -1;

        FD_ZERO(&rfds);
        FD_ZERO(&wfds);

        if (s_server.listen_fd >= 0) {
            FD_SET(s_server.listen_fd, &rfds);
            max_fd = s_server.listen_fd;
        }

        for (uint8_t i = 0; i < s_server.max_clients; ++i) {
            tcp_conn_t *conn = &s_server.slots[i];

            if (conn->state != TCP_SLOT_USED || conn->fd < 0) {
                continue;
            }

            if (!conn->close_after_drain) {
                FD_SET(conn->fd, &rfds);
            }
            if (conn->tx_offset < conn->tx_len) {
                FD_SET(conn->fd, &wfds);
            }

            if (conn->fd > max_fd) {
                max_fd = conn->fd;
            }
        }

        if (max_fd < 0) {
            break;
        }

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = TCP_SELECT_TIMEOUT_MS * 1000U;

        int ret = select(max_fd + 1, &rfds, &wfds, NULL, &tv);

        if (!s_server.running) {
            break;
        }

        if (ret < 0) {
            int err = errno;
            if (!tcp_is_retryable_socket_error(err)) {
                TCP_TRANSPORT_LOGE("select failed err=%d", err);
            }
            continue;
        }

        if (ret == 0) {
            tcp_check_idle_clients();
            continue;
        }

        if (s_server.listen_fd >= 0 && FD_ISSET(s_server.listen_fd, &rfds)) {
            tcp_handle_accept();
        }

        for (uint8_t i = 0; i < s_server.max_clients; ++i) {
            tcp_conn_t *conn = &s_server.slots[i];
            if (conn->state == TCP_SLOT_USED && conn->fd >= 0 && FD_ISSET(conn->fd, &rfds)) {
                tcp_handle_rx(conn);
            }
        }

        for (uint8_t i = 0; i < s_server.max_clients; ++i) {
            tcp_conn_t *conn = &s_server.slots[i];
            if (conn->state == TCP_SLOT_USED && conn->fd >= 0 && FD_ISSET(conn->fd, &wfds)) {
                tcp_handle_tx(conn);
            }
        }

        tcp_check_idle_clients();
    }

    tcp_close_all_clients();
    tcp_close_fd(&s_server.listen_fd);
    s_server.running = false;
    s_server.started = false;
    s_server.task_handle = NULL;

    TCP_TRANSPORT_LOGI("server stopped");
    vTaskDelete(NULL);
}

int tcp_server_start(uint16_t port, uint8_t max_clients,
                     const tcp_server_callbacks_t *callbacks)
{
    if (port == 0 || max_clients == 0 || max_clients > TCP_SERVER_MAX_CLIENTS) {
        return TCP_TRANSPORT_ERR_INVALID_ARG;
    }

    if (s_server.started) {
        return TCP_TRANSPORT_ERR_ALREADY_STARTED;
    }

    tcp_reset_all_slots();
    memset(&s_server.callbacks, 0, sizeof(s_server.callbacks));
    if (callbacks != NULL) {
        s_server.callbacks = *callbacks;
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_fd < 0) {
        TCP_TRANSPORT_LOGE("socket failed err=%d", errno);
        return TCP_TRANSPORT_ERR_SOCKET;
    }

#if TCP_ENABLE_REUSEADDR
    int reuse = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        TCP_TRANSPORT_LOGE("setsockopt SO_REUSEADDR failed err=%d", errno);
    }
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        TCP_TRANSPORT_LOGE("bind failed port=%u err=%d", (unsigned)port, errno);
        close(listen_fd);
        return TCP_TRANSPORT_ERR_BIND;
    }

    if (tcp_set_nonblocking(listen_fd) < 0) {
        TCP_TRANSPORT_LOGE("server nonblock failed err=%d", errno);
        close(listen_fd);
        return TCP_TRANSPORT_ERR_NONBLOCK;
    }

    if (listen(listen_fd, TCP_LISTEN_BACKLOG) < 0) {
        TCP_TRANSPORT_LOGE("listen failed err=%d", errno);
        close(listen_fd);
        return TCP_TRANSPORT_ERR_LISTEN;
    }

    s_server.listen_fd = listen_fd;
    s_server.port = port;
    s_server.max_clients = max_clients;
    s_server.running = true;
    s_server.started = true;

    portBASE_TYPE task_ret = xTaskCreate(tcp_network_task,
                                         "tcp_transport",
                                         TCP_NETWORK_TASK_STACK_SIZE,
                                         NULL,
                                         TCP_NETWORK_TASK_PRIORITY,
                                         &s_server.task_handle);
    if (task_ret != pdPASS) {
        TCP_TRANSPORT_LOGE("task create failed");
        s_server.running = false;
        s_server.started = false;
        s_server.task_handle = NULL;
        tcp_close_fd(&s_server.listen_fd);
        tcp_reset_all_slots();
        return TCP_TRANSPORT_ERR_TASK;
    }

    TCP_TRANSPORT_LOGI("server started port=%u max_clients=%u",
                       (unsigned)port,
                       (unsigned)max_clients);
    return TCP_TRANSPORT_OK;
}

int tcp_server_stop(void)
{
    if (!s_server.started) {
        return TCP_TRANSPORT_OK;
    }

    s_server.running = false;

    if (tcp_is_network_task()) {
        return TCP_TRANSPORT_OK;
    }

    for (uint8_t i = 0; i < 10U && s_server.task_handle != NULL; ++i) {
        vTaskDelay(pdMS_TO_TICKS(TCP_SELECT_TIMEOUT_MS));
    }

    if (s_server.task_handle != NULL) {
        TCP_TRANSPORT_LOGE("server stop timeout");
        return TCP_TRANSPORT_ERR_STOP_TIMEOUT;
    }

    return TCP_TRANSPORT_OK;
}

size_t tcp_send(tcp_conn_t *conn, const uint8_t *buf, size_t len)
{
    if (!tcp_is_network_task()
        || !tcp_conn_belongs_to_server(conn)
        || buf == NULL
        || len == 0
        || conn->close_after_drain
        || conn->state != TCP_SLOT_USED) {
        return 0;
    }

    if (conn->tx_offset > conn->tx_len) {
        conn->tx_len = 0;
        conn->tx_offset = 0;
    }

    if (conn->tx_offset > 0) {
        size_t remaining = conn->tx_len - conn->tx_offset;
        if (remaining > 0) {
            memmove(conn->tx_buf, &conn->tx_buf[conn->tx_offset], remaining);
        }
        conn->tx_len = remaining;
        conn->tx_offset = 0;
    }

    if (conn->tx_len >= sizeof(conn->tx_buf)) {
        TCP_TRANSPORT_LOGI("tx buffer full fd=%d", conn->fd);
        return 0;
    }

    size_t available = sizeof(conn->tx_buf) - conn->tx_len;
    size_t accepted = len < available ? len : available;

    memcpy(&conn->tx_buf[conn->tx_len], buf, accepted);
    conn->tx_len += accepted;

    if (accepted < len) {
        TCP_TRANSPORT_LOGI("tx buffer saturated fd=%d accepted=%u requested=%u",
                           conn->fd,
                           (unsigned)accepted,
                           (unsigned)len);
    }

    return accepted;
}

int tcp_close_after_drain(tcp_conn_t *conn)
{
    if (!tcp_is_network_task()
        || !tcp_conn_belongs_to_server(conn)
        || conn->state != TCP_SLOT_USED) {
        return TCP_TRANSPORT_ERR_INVALID_ARG;
    }

    if (conn->tx_offset >= conn->tx_len) {
        tcp_mark_tx_empty(conn);
        return TCP_TRANSPORT_OK;
    }

    conn->close_after_drain = true;
    return TCP_TRANSPORT_OK;
}

void tcp_transport_close(tcp_conn_t *conn)
{
    if (!tcp_is_network_task() || !tcp_conn_belongs_to_server(conn)) {
        return;
    }

    tcp_close_slot(conn, true);
}
