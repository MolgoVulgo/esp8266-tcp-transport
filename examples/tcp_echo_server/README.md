# TCP echo server example

Exemple PlatformIO pour ESP8266 RTOS SDK.

Il connecte l'ESP8266 en Wi-Fi station, demarre `esp8266-tcp-transport` sur `TCP_ECHO_PORT`, puis renvoie chaque bloc recu au client TCP.

## Configuration

Modifier les flags dans `platformio.ini` ou les surcharger localement :

```ini
[env:esp12e]
build_flags =
  -D WIFI_SSID=\"your-ssid\"
  -D WIFI_PASSWORD=\"your-password\"
  -D TCP_ECHO_PORT=7777
```

Ne pas commiter de vrais identifiants Wi-Fi.

## Build

Depuis la racine du depot :

```sh
pio run -d examples/tcp_echo_server
```

## Test manuel

Apres flash, relever l'adresse IP affichee sur le moniteur serie, puis lancer :

```sh
python3 examples/tcp_echo_server/tools/tcp_echo_client.py <ip-esp8266> 7777 "ping"
```

Resultat attendu :

```text
sent: 4 bytes
received: ping
```
