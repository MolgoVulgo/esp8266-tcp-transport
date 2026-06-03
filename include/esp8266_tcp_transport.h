#ifndef TCP_TRANSPORT_H
#define TCP_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TCP_SERVER_MAX_CLIENTS
#define TCP_SERVER_MAX_CLIENTS      3U
#endif

#ifndef TCP_RX_BUFFER_SIZE
#define TCP_RX_BUFFER_SIZE          512U
#endif

#ifndef TCP_TX_BUFFER_SIZE
#define TCP_TX_BUFFER_SIZE          512U
#endif

#ifndef TCP_SELECT_TIMEOUT_MS
#define TCP_SELECT_TIMEOUT_MS       100U
#endif

/* Idle timeout in milliseconds. Set to 0 to disable. */
#ifndef TCP_IDLE_TIMEOUT_MS
#define TCP_IDLE_TIMEOUT_MS         5000U
#endif

/* Public capability markers for client libraries. */
#define TCP_TRANSPORT_HAS_ON_DRAIN          1
#define TCP_TRANSPORT_HAS_CLOSE_AFTER_DRAIN 1
#define TCP_TRANSPORT_HAS_TX_AVAILABLE      1
#define TCP_TRANSPORT_HAS_REMOTE_ADDR       1
#define TCP_TRANSPORT_HAS_CLOSE_REASON      1
#define TCP_TRANSPORT_HAS_LAST_ERROR        1

typedef enum {
    TCP_SLOT_FREE = 0,
    TCP_SLOT_USED,
    TCP_SLOT_CLOSING,
    TCP_SLOT_ERROR
} tcp_slot_state_t;

typedef enum {
    TCP_TRANSPORT_OK = 0,
    TCP_TRANSPORT_ERR_INVALID_ARG = -1,
    TCP_TRANSPORT_ERR_ALREADY_STARTED = -2,
    TCP_TRANSPORT_ERR_SOCKET = -3,
    TCP_TRANSPORT_ERR_BIND = -4,
    TCP_TRANSPORT_ERR_LISTEN = -5,
    TCP_TRANSPORT_ERR_NONBLOCK = -6,
    TCP_TRANSPORT_ERR_TASK = -7,
    TCP_TRANSPORT_ERR_STOP_TIMEOUT = -8
} tcp_transport_result_t;

typedef enum {
    TCP_CLOSE_NONE = 0,
    TCP_CLOSE_REMOTE,
    TCP_CLOSE_LOCAL,
    TCP_CLOSE_AFTER_DRAIN,
    TCP_CLOSE_IDLE_TIMEOUT,
    TCP_CLOSE_SOCKET_ERROR,
    TCP_CLOSE_SERVER_STOP
} tcp_close_reason_t;

/* Exposed to callbacks for V1 diagnostics and bounded buffers.
 * Application code must not modify fields directly.
 */
typedef struct tcp_conn {
    int fd;
    uint32_t remote_ip;
    uint16_t remote_port;
    uint16_t local_port;

    uint8_t rx_buf[TCP_RX_BUFFER_SIZE];
    size_t rx_len;

    uint8_t tx_buf[TCP_TX_BUFFER_SIZE];
    size_t tx_len;
    size_t tx_offset;

    uint32_t last_activity_ms;
    tcp_slot_state_t state;
    tcp_close_reason_t close_reason;
    int last_error;
    bool close_after_drain;
} tcp_conn_t;

/* Callback contract:
 * - on_connect is called when a client is accepted.
 * - on_data is called for received data from the internal network task.
 * - on_drain is called only when tx_buf becomes fully empty after sending
 *   pending data. It is not called when close_after_drain triggers
 *   the final close.
 * - on_drain may call tcp_send() to continue a bounded streamed response.
 * - on_error is called for socket errors and is always followed by on_close.
 * - on_close is called once for the final connection close after on_connect
 *   and receives the effective close reason.
 * Callbacks run in the internal network task and must not block.
 */
typedef struct {
    void (*on_connect)(tcp_conn_t *conn);
    void (*on_data)(tcp_conn_t *conn, const uint8_t *buf, size_t len);
    void (*on_drain)(tcp_conn_t *conn);
    void (*on_close)(tcp_conn_t *conn, tcp_close_reason_t reason);
    void (*on_error)(tcp_conn_t *conn, int err);
} tcp_server_callbacks_t;

int tcp_server_start(uint16_t port, uint8_t max_clients,
                     const tcp_server_callbacks_t *callbacks);
int tcp_server_stop(void);

size_t tcp_send(tcp_conn_t *conn, const uint8_t *buf, size_t len);
size_t tcp_tx_available(const tcp_conn_t *conn);
bool tcp_tx_empty(const tcp_conn_t *conn);
int tcp_close_after_drain(tcp_conn_t *conn);
void tcp_transport_close(tcp_conn_t *conn);

static inline void tcp_close(tcp_conn_t *conn)
{
    tcp_transport_close(conn);
}

#ifdef __cplusplus
}
#endif

#endif
