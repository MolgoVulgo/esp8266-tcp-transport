# esp8266-tcp-transport

`esp8266-tcp-transport` est une petite bibliotheque PlatformIO pour les projets ESP8266 RTOS SDK.

Elle fournit une couche de transport TCP serveur bornee pour flux d'octets. Elle reste volontairement independante de HTTP et de la logique applicative.

## Fonctionnalites

- Une seule task FreeRTOS reseau interne.
- Sockets TCP non bloquants surveilles avec `select()`.
- Slots clients statiques, bornes par `TCP_SERVER_MAX_CLIENTS`.
- Buffers RX et TX statiques par client.
- Callbacks applicatifs courts pour connexion, reception, fermeture et erreur.
- Emission bufferisee non bloquante avec gestion des envois partiels.
- Timeout d'inactivite optionnel via `TCP_IDLE_TIMEOUT_MS`.

Non inclus : HTTP, TLS, WebSocket, UDP, IPv6, DNS, authentification, gestion de session ou queues applicatives.

## Installation

Ajouter la bibliotheque dans un projet PlatformIO ESP8266 RTOS SDK :

```ini
[env:esp12e]
platform = espressif8266
board = esp12e
framework = esp8266-rtos-sdk

lib_deps =
  https://github.com/MolgoVulgo/esp8266-tcp-transport.git
```

Inclure le header public :

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

En V1, `tcp_send()`, `tcp_close_after_drain()` et `tcp_close()` sont utilisables uniquement depuis la task reseau. En usage normal, ils sont appeles depuis `on_connect`, `on_data`, `on_close` ou `on_error`.

Les callbacks s'executent dans la task reseau interne. Ils ne doivent pas bloquer, attendre une ressource lente ou effectuer un traitement long.

## Exemple

Un exemple complet de serveur echo TCP est disponible dans :

```text
examples/tcp_echo_server
```

Configurer le Wi-Fi par flags de build. Ne pas commiter de vrais identifiants :

```ini
build_flags =
  -D WIFI_SSID=\"your-ssid\"
  -D WIFI_PASSWORD=\"your-password\"
  -D TCP_ECHO_PORT=7777
```

Compiler l'exemple :

```sh
pio run -d examples/tcp_echo_server
```

Apres flash de l'ESP8266 et lecture de son adresse IP dans le moniteur serie :

```sh
python3 examples/tcp_echo_server/tools/tcp_echo_client.py <ip-esp8266> 7777 "ping"
```

Resultat attendu : le client recoit les memes octets que ceux envoyes.

## Documentation

- [Documentation de reference](docs/tcp_transport_esp8266.fr.md)
- [Modele de rapport memoire](docs/tcp_transport_memory_report.md)
- [Plan de test](tests/tcp_transport_test_plan.md)

## Licence

Aucune licence n'est encore definie. Le champ `license` est volontairement absent de `library.json` jusqu'a choix explicite.
