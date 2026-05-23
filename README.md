# esp8266-tcp-transport

Bibliotheque PlatformIO pour ESP8266 RTOS SDK.

Elle fournit un transport TCP serveur minimal, borne et independant de HTTP :

- une seule task FreeRTOS reseau ;
- sockets non bloquants avec `select()` ;
- jusqu'a `TCP_SERVER_MAX_CLIENTS` clients statiques ;
- buffers RX/TX statiques par client ;
- callbacks applicatifs courts ;
- aucun TLS, HTTP, WebSocket, UDP, DNS ou gestion de session.

## Installation PlatformIO

Depuis un projet PlatformIO ESP8266 RTOS SDK :

```ini
[env:esp12e]
platform = espressif8266
board = esp12e
framework = esp8266-rtos-sdk

lib_deps =
  https://github.com/MolgoVulgo/esp8266-tcp-transport.git
```

Include cote application :

```c
#include "tcp_transport.h"
```

## API minimale

```c
int tcp_server_start(uint16_t port, uint8_t max_clients,
                     const tcp_server_callbacks_t *callbacks);
int tcp_server_stop(void);

size_t tcp_send(tcp_conn_t *conn, const uint8_t *buf, size_t len);
int tcp_close_after_drain(tcp_conn_t *conn);
void tcp_close(tcp_conn_t *conn);
```

En V1, `tcp_send()`, `tcp_close_after_drain()` et `tcp_close()` sont a appeler depuis la task reseau, typiquement depuis `on_data`, `on_connect`, `on_close` ou `on_error`.

Les callbacks ne doivent pas bloquer et ne doivent pas effectuer de traitement long.

## Fermeture apres vidage TX

Le transport expose `tcp_close_after_drain()` pour permettre a une couche superieure de demander la fermeture serveur apres emission complete des donnees deja bufferisees.

Si le buffer TX est vide, la fermeture est immediate.
Si le buffer TX contient encore des donnees, le transport poursuit les envois partiels puis ferme automatiquement la connexion lorsque `tx_offset == tx_len`.

Apres appel a `tcp_close_after_drain()`, aucun nouvel envoi applicatif n'est accepte sur cette connexion : `tcp_send()` retourne `0`.

## Exemple echo TCP

Un exemple complet est disponible dans :

```text
examples/tcp_echo_server
```

Configuration Wi-Fi par flags PlatformIO, sans secret commite :

```ini
build_flags =
  -D WIFI_SSID=\"your-ssid\"
  -D WIFI_PASSWORD=\"your-password\"
  -D TCP_ECHO_PORT=7777
```

Build de l'exemple :

```sh
pio run -d examples/tcp_echo_server
```

Apres flash et connexion Wi-Fi, tester depuis un poste du meme reseau :

```sh
python3 examples/tcp_echo_server/tools/tcp_echo_client.py <ip-esp8266> 7777 "ping"
```

Le client doit recevoir exactement les octets envoyes.

## Structure

```text
include/
  tcp_transport.h
src/
  tcp_transport.c
examples/
  tcp_echo_server/
docs/
tests/
```

La structure suit la recommandation PlatformIO : manifeste `library.json` a la racine, headers dans `include/`, sources dans `src/`, exemples dans `examples/`.

## Contraintes memoire

Par defaut :

- `TCP_SERVER_MAX_CLIENTS = 3`
- `TCP_RX_BUFFER_SIZE = 512`
- `TCP_TX_BUFFER_SIZE = 512`
- `TCP_SELECT_TIMEOUT_MS = 100`
- `TCP_NETWORK_TASK_STACK_SIZE = 1024`

Le rapport de mesure cible est a remplir dans `docs/tcp_transport_memory_report.md` apres build et test sur materiel.

## Licence

Licence non definie pour le moment. Le champ `license` est volontairement absent de `library.json` jusqu'a choix explicite.
