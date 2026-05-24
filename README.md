# esp8266-tcp-transport

`esp8266-tcp-transport` is a small PlatformIO library for ESP8266 RTOS SDK projects.

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

Add the library to a PlatformIO ESP8266 RTOS SDK project:

```ini
[env:esp12e]
platform = espressif8266
board = esp12e
framework = esp8266-rtos-sdk

lib_deps =
  https://github.com/MolgoVulgo/esp8266-tcp-transport.git
```

Include the public header:

```c
#include "tcp_transport.h"
```

## Minimal API

```c
int tcp_server_start(uint16_t port, uint8_t max_clients,
                     const tcp_server_callbacks_t *callbacks);
int tcp_server_stop(void);

size_t tcp_send(tcp_conn_t *conn, const uint8_t *buf, size_t len);
int tcp_close_after_drain(tcp_conn_t *conn);
void tcp_close(tcp_conn_t *conn);
```

`tcp_send()`, `tcp_close_after_drain()` and `tcp_close()` are V1 network-task-only APIs. In normal use they are called from `on_connect`, `on_data`, `on_close` or `on_error`.

Callbacks run inside the internal network task. They must not block, wait on slow resources or perform long processing.

## Example

A complete TCP echo example is available in:

```text
examples/tcp_echo_server
```

Configure Wi-Fi through build flags. Do not commit real credentials:

```ini
build_flags =
  -D WIFI_SSID=\"your-ssid\"
  -D WIFI_PASSWORD=\"your-password\"
  -D TCP_ECHO_PORT=7777
```

Build the example:

```sh
pio run -d examples/tcp_echo_server
```

After flashing the ESP8266 and reading its IP address from the serial monitor:

```sh
python3 examples/tcp_echo_server/tools/tcp_echo_client.py <esp8266-ip> 7777 "ping"
```

Expected result: the client receives the same bytes it sent.

## Documentation

- [Reference documentation](docs/tcp_transport_esp8266.md)
- [Memory report template](docs/tcp_transport_memory_report.md)
- [Test plan](tests/tcp_transport_test_plan.md)

## License

No license is defined yet. The `license` field is intentionally absent from `library.json` until an explicit choice is made.
