#!/usr/bin/env python3
import socket
import sys


def main() -> int:
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} <host> <port> <message>", file=sys.stderr)
        return 2

    host = sys.argv[1]
    port = int(sys.argv[2])
    payload = sys.argv[3].encode("utf-8")

    with socket.create_connection((host, port), timeout=5) as sock:
        sock.sendall(payload)
        received = sock.recv(len(payload))

    if received != payload:
        print(f"mismatch: expected {payload!r}, got {received!r}", file=sys.stderr)
        return 1

    print(f"sent: {len(payload)} bytes")
    print(f"received: {received.decode('utf-8', errors='replace')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
