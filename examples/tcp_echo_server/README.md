# TCP Echo Server Example / Exemple serveur echo TCP

## English

PlatformIO example for ESP8266 RTOS SDK.

The firmware connects the ESP8266 as a Wi-Fi station, starts `esp8266-tcp-transport` on `TCP_ECHO_PORT`, then sends each received TCP block back to the client.

### Configuration

Edit the flags in `platformio.ini` or override them locally:

```ini
[env:esp12e]
build_flags =
  -D WIFI_SSID=\"your-ssid\"
  -D WIFI_PASSWORD=\"your-password\"
  -D TCP_ECHO_PORT=7777
```

Do not commit real Wi-Fi credentials.

### Build

From the repository root:

```sh
pio run -d examples/tcp_echo_server
```

### Manual Test

After flashing, read the IP address from the serial monitor, then run:

```sh
python3 examples/tcp_echo_server/tools/tcp_echo_client.py <esp8266-ip> 7777 "ping"
```

Expected result:

```text
sent: 4 bytes
received: ping
```

## Francais

Exemple PlatformIO pour ESP8266 RTOS SDK.

Le firmware connecte l'ESP8266 en Wi-Fi station, demarre `esp8266-tcp-transport` sur `TCP_ECHO_PORT`, puis renvoie chaque bloc TCP recu au client.

### Configuration

Modifier les flags dans `platformio.ini` ou les surcharger localement :

```ini
[env:esp12e]
build_flags =
  -D WIFI_SSID=\"your-ssid\"
  -D WIFI_PASSWORD=\"your-password\"
  -D TCP_ECHO_PORT=7777
```

Ne pas commiter de vrais identifiants Wi-Fi.

### Build

Depuis la racine du depot :

```sh
pio run -d examples/tcp_echo_server
```

### Test manuel

Apres flash, relever l'adresse IP affichee sur le moniteur serie, puis lancer :

```sh
python3 examples/tcp_echo_server/tools/tcp_echo_client.py <ip-esp8266> 7777 "ping"
```

Resultat attendu :

```text
sent: 4 bytes
received: ping
```
