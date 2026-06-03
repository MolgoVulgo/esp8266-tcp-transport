# Changelog

## v2.0.0

Release target for the `IDF` branch promotion to `main`.

### Breaking changes

- public header renamed from `tcp_transport.h` to `esp8266_tcp_transport.h`
- implementation file renamed from `src/tcp_transport.c` to `src/esp8266_tcp_transport.c`
- `on_close` callback signature changed from:

```c
void (*on_close)(tcp_conn_t *conn);
```

to:

```c
void (*on_close)(tcp_conn_t *conn, tcp_close_reason_t reason);
```

- `tcp_conn_t` now exposes additional diagnostic fields:
  - `remote_ip`
  - `remote_port`
  - `local_port`
  - `close_reason`
  - `last_error`

### Added

- TX capability markers:
  - `TCP_TRANSPORT_HAS_TX_AVAILABLE`
  - `TCP_TRANSPORT_HAS_REMOTE_ADDR`
  - `TCP_TRANSPORT_HAS_CLOSE_REASON`
  - `TCP_TRANSPORT_HAS_LAST_ERROR`
- Wi-Fi enabled native ESP8266 RTOS SDK echo example
- host contract coverage for close reasons and diagnostics

### Validation

- host contract tests passing
- manual TCP echo test validated on target

## v1.0.2

Previous stable release line before the IDF branch promotion.
