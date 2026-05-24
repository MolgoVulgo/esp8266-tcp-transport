# Reference Documentation - ESP8266 RTOS SDK TCP Transport

## Purpose

`esp8266-tcp-transport` provides a TCP server transport component for ESP8266 RTOS SDK.

The module transports TCP byte streams. It does not know about HTTP, routes, sessions, web content or application logic.

Main goals:

- provide a minimal reusable TCP server;
- keep RAM usage bounded;
- avoid the one-task-per-client model;
- hide lwIP from application layers;
- expose a simple C API.

## Architecture

Target stack:

```text
lwIP / FreeRTOS
    |
tcp_transport
    |
optional protocol layer
    |
application
```

The transport uses one internal FreeRTOS task. This task monitors the server socket and client sockets with `select()`.

Sockets are non-blocking. Client connections are stored in static slots. Client events are processed sequentially inside the network task.

## V1 Scope

Included:

- TCP server only;
- non-blocking IPv4 TCP sockets;
- one FreeRTOS network task;
- `select()` loop;
- static client slots;
- static RX/TX buffers per client;
- global callbacks;
- buffered non-blocking transmit;
- immediate close or close after TX drain;
- configurable idle timeout;
- logs for start, stop, connection, rejection, error, close and TX saturation.

Out of scope:

- HTTP;
- TLS;
- WebSocket;
- UDP;
- IPv6;
- DNS;
- authentication;
- session management;
- application queue;
- guaranteed cross-task calls;
- dynamic allocation per client.

## Public Configuration

Main constants are defined in `include/tcp_transport.h`.

| Symbol | Default | Role |
|---|---:|---|
| `TCP_SERVER_MAX_CLIENTS` | `3` | Maximum number of static client slots |
| `TCP_RX_BUFFER_SIZE` | `512` | Receive buffer size per client |
| `TCP_TX_BUFFER_SIZE` | `512` | Transmit buffer size per client |
| `TCP_SELECT_TIMEOUT_MS` | `100` | Wake-up timeout for the `select()` loop |
| `TCP_IDLE_TIMEOUT_MS` | `5000` | Client idle timeout, `0` disables it |

Internal overrideable constants in `src/tcp_transport.c`:

| Symbol | Default | Role |
|---|---:|---|
| `TCP_NETWORK_TASK_STACK_SIZE` | `1024` | Network task stack, in FreeRTOS words |
| `TCP_NETWORK_TASK_PRIORITY` | `5` | Network task priority |
| `TCP_LISTEN_BACKLOG` | `TCP_SERVER_MAX_CLIENTS` | Backlog passed to `listen()` |
| `TCP_TRANSPORT_STACK_CHECK` | `0` | Enables low stack watermark logging |
| `TCP_ENABLE_REUSEADDR` | SDK-dependent | Enables `SO_REUSEADDR` when available |

Any change to buffer sizes, stack size or client count must be measured on target hardware.

## Memory Model

The server uses one static global state:

- one server socket descriptor;
- global callbacks;
- network task handle;
- an array of `TCP_SERVER_MAX_CLIENTS` `tcp_conn_t` slots.

Each client slot contains:

- client fd;
- RX buffer;
- TX buffer;
- RX/TX lengths and offsets;
- last activity timestamp;
- slot state;
- `close_after_drain` flag.

With default values, buffers account for about 3 KB for 3 clients, excluding slot structure overhead, FreeRTOS stack and internal lwIP memory.

Real measurements must be documented in `docs/tcp_transport_memory_report.md`.

## Connection Lifecycle

| State | Meaning |
|---|---|
| `TCP_SLOT_FREE` | Available slot |
| `TCP_SLOT_USED` | Active connection |
| `TCP_SLOT_CLOSING` | Close in progress |
| `TCP_SLOT_ERROR` | Error detected before close |

Nominal flow:

1. `accept()` creates a client fd.
2. The fd is switched to non-blocking mode.
3. A free slot is initialized.
4. `on_connect` is called if defined.
5. Receive events call `on_data`.
6. TX data is sent progressively.
7. Close calls `on_close` once.
8. The slot returns to `TCP_SLOT_FREE`.

When no slot is available, the client is accepted and immediately closed. The TCP/lwIP backlog is not used as an application-level regulation mechanism.

## Callback Contract

```c
typedef struct {
    void (*on_connect)(tcp_conn_t *conn);
    void (*on_data)(tcp_conn_t *conn, const uint8_t *buf, size_t len);
    void (*on_close)(tcp_conn_t *conn);
    void (*on_error)(tcp_conn_t *conn, int err);
} tcp_server_callbacks_t;
```

Rules:

- callbacks run inside the internal network task;
- callbacks must not block;
- callbacks must not wait on slow resources;
- callbacks must not perform long processing;
- callbacks must not expose lwIP to the application layer.

`on_error` is followed by connection close and `on_close`.

## Function Catalogue

### `tcp_server_start`

```c
int tcp_server_start(uint16_t port, uint8_t max_clients,
                     const tcp_server_callbacks_t *callbacks);
```

Starts the TCP server.

Parameters:

- `port`: local TCP port. `0` is invalid.
- `max_clients`: runtime client limit. Must be between `1` and `TCP_SERVER_MAX_CLIENTS`.
- `callbacks`: optional pointer to application callbacks. `NULL` is accepted.

Behavior:

- initializes slots;
- creates the server socket;
- binds to `INADDR_ANY:port`;
- switches the server socket to non-blocking mode;
- calls `listen()`;
- starts the internal network task.

Returns:

| Code | Meaning |
|---|---|
| `TCP_TRANSPORT_OK` | Server started |
| `TCP_TRANSPORT_ERR_INVALID_ARG` | Invalid port or client limit |
| `TCP_TRANSPORT_ERR_ALREADY_STARTED` | Server already started |
| `TCP_TRANSPORT_ERR_SOCKET` | Socket creation failed |
| `TCP_TRANSPORT_ERR_BIND` | `bind()` failed |
| `TCP_TRANSPORT_ERR_LISTEN` | `listen()` failed |
| `TCP_TRANSPORT_ERR_NONBLOCK` | Non-blocking setup failed |
| `TCP_TRANSPORT_ERR_TASK` | Task creation failed |

### `tcp_server_stop`

```c
int tcp_server_stop(void);
```

Requests TCP server stop.

Behavior:

- ends the network loop;
- closes all active clients;
- closes the server socket;
- restores internal state so the server can be started again.

If called from the network task, the function requests stop and returns without waiting for task deletion.

Returns:

| Code | Meaning |
|---|---|
| `TCP_TRANSPORT_OK` | Server stopped or already stopped |
| `TCP_TRANSPORT_ERR_STOP_TIMEOUT` | Network task did not confirm stop within the expected delay |

### `tcp_send`

```c
size_t tcp_send(tcp_conn_t *conn, const uint8_t *buf, size_t len);
```

Copies bytes into the connection TX buffer.

Parameters:

- `conn`: active connection owned by the server.
- `buf`: data to send.
- `len`: requested byte count.

Behavior:

- does not block;
- accepts only the available space in `tx_buf`;
- returns the number of accepted bytes;
- compacts the TX buffer if previous bytes have already been sent;
- refuses new sends after `tcp_close_after_drain()`.

Return:

- `0` if the call is invalid, the buffer is full or no byte can be accepted;
- `1..len` depending on available space.

V1 constraint: callable only from the internal network task.

### `tcp_close_after_drain`

```c
int tcp_close_after_drain(tcp_conn_t *conn);
```

Requests server-side close after all bytes already accepted in the TX buffer have been sent.

Behavior:

- closes immediately if TX is empty;
- sets `close_after_drain` if TX still contains data;
- refuses all later `tcp_send()` calls;
- closes automatically when `tx_offset == tx_len`.

Returns:

| Code | Meaning |
|---|---|
| `TCP_TRANSPORT_OK` | Request accepted |
| `TCP_TRANSPORT_ERR_INVALID_ARG` | Invalid connection or call outside the network task |

V1 constraint: callable only from the internal network task.

### `tcp_transport_close`

```c
void tcp_transport_close(tcp_conn_t *conn);
```

Immediately closes an active connection and frees its slot.

Behavior:

- closes the client fd;
- calls `on_close` if applicable;
- returns the slot to `TCP_SLOT_FREE`.

Invalid calls are ignored.

V1 constraint: callable only from the internal network task.

### `tcp_close`

```c
static inline void tcp_close(tcp_conn_t *conn);
```

Public inline alias for `tcp_transport_close()`.

Use `tcp_close()` in application code. `tcp_transport_close()` remains exposed to keep an explicit C symbol.

## Return Codes

```c
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
```

## Minimal Example

```c
static void on_data(tcp_conn_t *conn, const uint8_t *buf, size_t len)
{
    (void)tcp_send(conn, buf, len);
}

static tcp_server_callbacks_t callbacks = {
    .on_data = on_data,
};

void app_start_tcp(void)
{
    int ret = tcp_server_start(7777, TCP_SERVER_MAX_CLIENTS, &callbacks);
    if (ret != TCP_TRANSPORT_OK) {
        /* handle startup error */
    }
}
```

## Validation

Minimum expected validation:

- PlatformIO build;
- server start;
- single client connection;
- data receive;
- transmit through `tcp_send()`;
- remote close;
- TX saturation;
- excess client rejection;
- server stop;
- server restart;
- heap and stack measurement on target.

The detailed test plan is in `tests/tcp_transport_test_plan.md`.

## Known Limits

- No guaranteed cross-task API in V1.
- No application queue.
- No advanced backpressure.
- Long callbacks delay every client.
- Target memory measurements still need to be filled in the dedicated report.
