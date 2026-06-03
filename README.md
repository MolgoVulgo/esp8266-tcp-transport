# esp8266-tcp-transport

`esp8266-tcp-transport` is a native ESP8266 RTOS SDK component for ESP8266 RTOS SDK projects.

It provides a bounded TCP server transport layer for byte streams. It is intentionally independent from HTTP and application logic.

## Features

- One internal FreeRTOS network task.
- Non-blocking TCP sockets monitored with `select()`.
- Static client slots, bounded by `TCP_SERVER_MAX_CLIENTS`.
- Static RX and TX buffers per client.
- Short application callbacks for connection, data, close and error events.
- Non-blocking buffered transmit with partial-send handling.
- Optional idle timeout through `TCP_IDLE_TIMEOUT_MS`.

Not included: HTTP, TLS, WebSocket, UDP, IPv6, DNS, authentication, session management or application queues.

## Installation

Use this repository as the component root in an ESP8266 RTOS SDK project, for example through:

```text
<project>/components/esp8266-tcp-transport
```

Include the public header from the component:

```c
#include "tcp_transport.h"
```

## Minimal API

```c
int tcp_server_start(uint16_t port, uint8_t max_clients,
                     const tcp_server_callbacks_t *callbacks);
int tcp_server_stop(void);

size_t tcp_send(tcp_conn_t *conn, const uint8_t *buf, size_t len);
size_t tcp_tx_available(const tcp_conn_t *conn);
bool tcp_tx_empty(const tcp_conn_t *conn);
int tcp_close_after_drain(tcp_conn_t *conn);
void tcp_close(tcp_conn_t *conn);
```

`tcp_send()`, `tcp_close_after_drain()` and `tcp_close()` are network-task-only APIs. In normal use they are called from `on_connect`, `on_data` or `on_drain`. `on_close` and `on_error` should stay short and should not start long application logic.

`on_drain(conn)` means the internal TX buffer became empty after sending previously accepted bytes. It is not called when `close_after_drain` triggers the final close. It may call `tcp_send()` to queue the next chunk. It runs in the internal network task and must not block.

TX helpers:
- `tcp_tx_available(conn)` returns free space in the internal TX buffer, accounting for bytes already sent via `tx_offset`. It returns `0` for invalid connections, closed slots or when `close_after_drain` is active.
- `tcp_tx_empty(conn)` returns `true` when no byte is pending for transmit. It also returns `true` for invalid or unused connections.

Callbacks run inside the internal network task. They must not block, wait on slow resources or perform long processing.

## Example

A native ESP8266 RTOS SDK example app is available in:

```text
examples/tcp_echo_server
```

Build the example:

```sh
idf.py -C examples/tcp_echo_server build
```

Flash from the same directory:

```sh
idf.py -C examples/tcp_echo_server flash monitor
```

Then test the echo server from a host once the device is reachable on the network:

```sh
python3 examples/tcp_echo_server/tools/tcp_echo_client.py <esp8266-ip> 7777 "ping"
```

## Documentation

- [Reference documentation](docs/tcp_transport_esp8266.md)
- [Memory report template](docs/tcp_transport_memory_report.md)
- [Test plan](tests/tcp_transport_test_plan.md)

## License

No license is defined yet.
