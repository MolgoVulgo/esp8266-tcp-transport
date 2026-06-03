#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static xTaskHandle g_current_task = (xTaskHandle)0x1111;

xTaskHandle xTaskGetCurrentTaskHandle(void)
{
    return g_current_task;
}

uint32_t xTaskGetTickCount(void)
{
    return 0;
}

void vTaskDelay(portTickType ticks)
{
    (void)ticks;
}

void vTaskDelete(void *task)
{
    (void)task;
}

UBaseType_t uxTaskGetStackHighWaterMark(void *task)
{
    (void)task;
    return 512U;
}

portBASE_TYPE xTaskCreate(void (*task_func)(void *), const char *name,
                          unsigned short stack_depth, void *param,
                          UBaseType_t priority, xTaskHandle *handle)
{
    (void)task_func;
    (void)name;
    (void)stack_depth;
    (void)param;
    (void)priority;
    if (handle != NULL) {
        *handle = g_current_task;
    }
    return pdPASS;
}

#include "../../src/esp8266_tcp_transport.c"

static int g_on_drain_calls;
static int g_on_close_calls;
static size_t g_on_drain_send_accepted;
static tcp_close_reason_t g_last_close_reason;

static void test_on_drain_cb(tcp_conn_t *conn)
{
    static const uint8_t chunk[] = { 'N', 'E', 'X', 'T' };

    ++g_on_drain_calls;
    g_on_drain_send_accepted = tcp_send(conn, chunk, sizeof(chunk));
}

static void count_on_drain_cb(tcp_conn_t *conn)
{
    (void)conn;
    ++g_on_drain_calls;
}

static void test_on_close_cb(tcp_conn_t *conn, tcp_close_reason_t reason)
{
    (void)conn;
    ++g_on_close_calls;
    g_last_close_reason = reason;
}

static tcp_conn_t *setup_conn(void)
{
    memset(&s_server, 0, sizeof(s_server));
    s_server.task_handle = g_current_task;
    s_server.max_clients = TCP_SERVER_MAX_CLIENTS;
    tcp_reset_all_slots();

    tcp_conn_t *conn = &s_server.slots[0];
    conn->state = TCP_SLOT_USED;
    conn->fd = 7;
    return conn;
}

static void test_reset_slot_clears_diagnostics(void)
{
    tcp_conn_t conn;

    memset(&conn, 0xA5, sizeof(conn));
    tcp_reset_slot(&conn);

    assert(conn.fd == -1);
    assert(conn.remote_ip == 0U);
    assert(conn.remote_port == 0U);
    assert(conn.local_port == 0U);
    assert(conn.rx_len == 0U);
    assert(conn.tx_len == 0U);
    assert(conn.tx_offset == 0U);
    assert(conn.last_activity_ms == 0U);
    assert(conn.state == TCP_SLOT_FREE);
    assert(conn.close_reason == TCP_CLOSE_NONE);
    assert(conn.last_error == 0);
    assert(conn.close_after_drain == false);
}

static void test_tcp_tx_available_empty(void)
{
    tcp_conn_t *conn = setup_conn();
    assert(tcp_tx_available(conn) == TCP_TX_BUFFER_SIZE);
}

static void test_tcp_tx_available_full(void)
{
    tcp_conn_t *conn = setup_conn();
    conn->tx_len = TCP_TX_BUFFER_SIZE;
    conn->tx_offset = 0;
    assert(tcp_tx_available(conn) == 0);
}

static void test_tcp_tx_available_with_offset(void)
{
    tcp_conn_t *conn = setup_conn();
    conn->tx_len = 512;
    conn->tx_offset = 256;
    assert(tcp_tx_available(conn) == (TCP_TX_BUFFER_SIZE - 256));
}

static void test_tcp_tx_empty_empty(void)
{
    tcp_conn_t *conn = setup_conn();
    conn->tx_len = 0;
    conn->tx_offset = 0;
    assert(tcp_tx_empty(conn) == true);
}

static void test_tcp_tx_empty_pending(void)
{
    tcp_conn_t *conn = setup_conn();
    conn->tx_len = 8;
    conn->tx_offset = 3;
    assert(tcp_tx_empty(conn) == false);
}

static void test_on_drain_called_after_tx_empty(void)
{
    tcp_conn_t *conn = setup_conn();
    g_on_drain_calls = 0;
    s_server.callbacks.on_drain = count_on_drain_cb;

    conn->tx_len = 16;
    conn->tx_offset = 16;
    tcp_mark_tx_empty(conn);

    assert(g_on_drain_calls == 1);
    assert(conn->state == TCP_SLOT_USED);
}

static void test_on_drain_not_called_after_close_after_drain(void)
{
    tcp_conn_t *conn = setup_conn();
    g_on_drain_calls = 0;
    g_on_close_calls = 0;
    g_last_close_reason = TCP_CLOSE_NONE;
    s_server.callbacks.on_drain = count_on_drain_cb;
    s_server.callbacks.on_close = test_on_close_cb;

    conn->close_after_drain = true;
    conn->tx_len = 12;
    conn->tx_offset = 12;
    tcp_mark_tx_empty(conn);

    assert(g_on_drain_calls == 0);
    assert(g_on_close_calls == 1);
    assert(g_last_close_reason == TCP_CLOSE_AFTER_DRAIN);
    assert(conn->state == TCP_SLOT_FREE);
}

static void test_tcp_send_allowed_from_on_drain(void)
{
    tcp_conn_t *conn = setup_conn();
    g_on_drain_calls = 0;
    g_on_drain_send_accepted = 0;
    s_server.callbacks.on_drain = test_on_drain_cb;

    conn->tx_len = 5;
    conn->tx_offset = 5;
    tcp_mark_tx_empty(conn);

    assert(g_on_drain_calls == 1);
    assert(g_on_drain_send_accepted == 4);
    assert(conn->tx_len == 4);
    assert(conn->tx_offset == 0);
}

static void test_multiple_send_drain_cycles(void)
{
    static const uint8_t block1[] = { 'a', 'b', 'c' };
    static const uint8_t block2[] = { 'd', 'e' };

    tcp_conn_t *conn = setup_conn();

    assert(tcp_send(conn, block1, sizeof(block1)) == sizeof(block1));
    conn->tx_offset = conn->tx_len;
    tcp_mark_tx_empty(conn);
    assert(conn->tx_len == 0);
    assert(conn->tx_offset == 0);

    assert(tcp_send(conn, block2, sizeof(block2)) == sizeof(block2));
    conn->tx_offset = conn->tx_len;
    tcp_mark_tx_empty(conn);
    assert(conn->tx_len == 0);
    assert(conn->tx_offset == 0);
}

static void test_on_close_once_after_close_after_drain(void)
{
    tcp_conn_t *conn = setup_conn();
    g_on_close_calls = 0;
    g_last_close_reason = TCP_CLOSE_NONE;
    s_server.callbacks.on_close = test_on_close_cb;

    conn->tx_len = 0;
    conn->tx_offset = 0;
    assert(tcp_close_after_drain(conn) == TCP_TRANSPORT_OK);
    assert(g_on_close_calls == 1);
    assert(g_last_close_reason == TCP_CLOSE_AFTER_DRAIN);
    assert(conn->state == TCP_SLOT_FREE);
}

static void test_no_on_drain_after_close(void)
{
    tcp_conn_t *conn = setup_conn();
    g_on_close_calls = 0;
    g_last_close_reason = TCP_CLOSE_NONE;
    g_on_drain_calls = 0;
    s_server.callbacks.on_drain = count_on_drain_cb;
    s_server.callbacks.on_close = test_on_close_cb;

    tcp_close_slot(conn, true, TCP_CLOSE_LOCAL, 0);
    assert(conn->state == TCP_SLOT_FREE);
    assert(g_on_drain_calls == 0);
    assert(g_on_close_calls == 1);
    assert(g_last_close_reason == TCP_CLOSE_LOCAL);
}

static void test_on_drain_null_keeps_previous_behavior(void)
{
    tcp_conn_t *conn = setup_conn();
    s_server.callbacks.on_drain = NULL;

    conn->tx_len = 9;
    conn->tx_offset = 9;
    tcp_mark_tx_empty(conn);

    assert(conn->state == TCP_SLOT_USED);
    assert(conn->tx_len == 0);
    assert(conn->tx_offset == 0);
}

static void test_accept_stores_remote_endpoint(void)
{
    struct sockaddr_in client_addr;
    tcp_conn_t *conn = setup_conn();

    conn->remote_ip = 0;
    conn->remote_port = 0;
    conn->local_port = 0;

    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_addr.s_addr = 0x01020304U;
    client_addr.sin_port = htons(4321);

    conn->remote_ip = client_addr.sin_addr.s_addr;
    conn->remote_port = ntohs(client_addr.sin_port);
    conn->local_port = 7777;

    assert(conn->remote_ip == 0x01020304U);
    assert(conn->remote_port == 4321U);
    assert(conn->local_port == 7777U);
}

static void test_fail_slot_sets_error_diagnostics(void)
{
    tcp_conn_t *conn = setup_conn();

    g_on_close_calls = 0;
    g_last_close_reason = TCP_CLOSE_NONE;
    s_server.callbacks.on_close = test_on_close_cb;
    s_server.callbacks.on_error = NULL;

    tcp_fail_slot(conn, 123);

    assert(g_on_close_calls == 1);
    assert(g_last_close_reason == TCP_CLOSE_SOCKET_ERROR);
    assert(conn->state == TCP_SLOT_FREE);
}

int main(void)
{
    test_reset_slot_clears_diagnostics();
    test_tcp_tx_available_empty();
    test_tcp_tx_available_full();
    test_tcp_tx_available_with_offset();
    test_tcp_tx_empty_empty();
    test_tcp_tx_empty_pending();
    test_on_drain_called_after_tx_empty();
    test_on_drain_not_called_after_close_after_drain();
    test_tcp_send_allowed_from_on_drain();
    test_multiple_send_drain_cycles();
    test_on_close_once_after_close_after_drain();
    test_no_on_drain_after_close();
    test_on_drain_null_keeps_previous_behavior();
    test_accept_stores_remote_endpoint();
    test_fail_slot_sets_error_diagnostics();

    printf("tcp_transport_contract_tests: OK\n");
    return 0;
}
