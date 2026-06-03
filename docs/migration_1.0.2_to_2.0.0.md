# Migration 1.0.2 to 2.0.0

## Scope

This migration applies when moving from the `1.0.2` release line to the IDF-based `2.0.0` line.

## Required source changes

### 1. Update the public include

Replace:

```c
#include "tcp_transport.h"
```

with:

```c
#include "esp8266_tcp_transport.h"
```

### 2. Update the close callback signature

Replace:

```c
static void on_close(tcp_conn_t *conn)
{
    (void)conn;
}
```

with:

```c
static void on_close(tcp_conn_t *conn, tcp_close_reason_t reason)
{
    (void)conn;
    (void)reason;
}
```

### 3. Use the new diagnostics if needed

During `on_connect`, `on_error`, and `on_close`, the following fields are available:

- `conn->remote_ip`
- `conn->remote_port`
- `conn->local_port`
- `conn->close_reason`
- `conn->last_error`

## Close reason mapping

| Reason | Meaning |
|---|---|
| `TCP_CLOSE_REMOTE` | peer closed the connection |
| `TCP_CLOSE_LOCAL` | application called `tcp_close()` |
| `TCP_CLOSE_AFTER_DRAIN` | close requested after pending TX drain |
| `TCP_CLOSE_IDLE_TIMEOUT` | idle timeout closed the connection |
| `TCP_CLOSE_SOCKET_ERROR` | non-retryable socket error |
| `TCP_CLOSE_SERVER_STOP` | server stop closed the connection |

## Validation checklist

- update includes to `esp8266_tcp_transport.h`
- rebuild all modules using `tcp_server_callbacks_t`
- check every `on_close` implementation
- rerun host contract tests
- rerun target echo/manual transport tests
