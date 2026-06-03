#!/usr/bin/env sh
set -eu

cc -std=c99 -Wall -Wextra -Werror \
  -Itests/host_stubs \
  -Iinclude \
  tests/host/test_tcp_transport_contract.c \
  -o /tmp/tcp_transport_contract_tests

/tmp/tcp_transport_contract_tests
