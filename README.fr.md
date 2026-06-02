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
size_t tcp_tx_available(const tcp_conn_t *conn);
bool tcp_tx_empty(const tcp_conn_t *conn);
int tcp_close_after_drain(tcp_conn_t *conn);
void tcp_close(tcp_conn_t *conn);
```

`tcp_send()`, `tcp_close_after_drain()` et `tcp_close()` sont des API utilisables uniquement depuis la task reseau interne. En usage normal, elles sont appelees depuis `on_connect`, `on_data` ou `on_drain`. `on_close` et `on_error` doivent rester courts et ne pas lancer de logique applicative longue.

`on_drain(conn)` signifie que le buffer TX interne est devenu vide apres envoi des octets precedemment acceptes. Il n'est pas appele si `close_after_drain` declenche la fermeture finale. Il peut appeler `tcp_send()` pour pousser le bloc suivant. Il s'execute dans la task reseau interne et ne doit pas bloquer.

Helpers TX :
- `tcp_tx_available(conn)` retourne l'espace disponible dans le buffer TX interne en tenant compte des octets deja emis via `tx_offset`. Retourne `0` si connexion invalide, slot ferme ou `close_after_drain` actif.
- `tcp_tx_empty(conn)` retourne `true` si aucun octet n'est en attente d'emission. Retourne aussi `true` si connexion invalide ou slot non utilise.

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
