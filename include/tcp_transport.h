#ifndef TCP_TRANSPORT_H
#define TCP_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TCP_SERVER_MAX_CLIENTS      3U
#define TCP_RX_BUFFER_SIZE          512U
#define TCP_TX_BUFFER_SIZE          512U
#define TCP_SELECT_TIMEOUT_MS       100U

/* Disabled by default until a project-level value is chosen from measurements. */
#ifndef TCP_IDLE_TIMEOUT_MS
#define TCP_IDLE_TIMEOUT_MS         0U
#endif

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
    TCP_TRANSPORT_ERR_TASK = -7
} tcp_transport_result_t;

/* Exposed to callbacks for V1 diagnostics and bounded buffers.
 * Application code must not modify fields directly.
 */
typedef struct tcp_conn {
    int fd;

    uint8_t rx_buf[TCP_RX_BUFFER_SIZE];
    size_t rx_len;

    uint8_t tx_buf[TCP_TX_BUFFER_SIZE];
    size_t tx_len;
    size_t tx_offset;

    uint32_t last_activity_ms;
    tcp_slot_state_t state;
    bool close_after_drain;
} tcp_conn_t;

typedef struct {
    void (*on_connect)(tcp_conn_t *conn);
    void (*on_data)(tcp_conn_t *conn, const uint8_t *buf, size_t len);
    void (*on_close)(tcp_conn_t *conn);
    void (*on_error)(tcp_conn_t *conn, int err);
} tcp_server_callbacks_t;

int tcp_server_start(uint16_t port, uint8_t max_clients,
                     const tcp_server_callbacks_t *callbacks);
void tcp_server_stop(void);

size_t tcp_send(tcp_conn_t *conn, const uint8_t *buf, size_t len);
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
