# esp8266-tcp-transport

`esp8266-tcp-transport` est un composant natif ESP8266 RTOS SDK pour les projets ESP8266 RTOS SDK.

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

Utiliser ce depot comme racine du composant dans un projet ESP8266 RTOS SDK, par exemple sous :

```text
<projet>/components/esp8266-tcp-transport
```

Inclure le header public du composant :

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

Une application d'exemple native ESP8266 RTOS SDK est disponible dans :

```text
examples/tcp_echo_server
```

Compiler l'exemple :

```sh
idf.py -C examples/tcp_echo_server build
```

Flasher depuis le meme dossier :

```sh
idf.py -C examples/tcp_echo_server flash monitor
```

Puis tester l'echo depuis une machine hote une fois l'equipement joignable :

```sh
python3 examples/tcp_echo_server/tools/tcp_echo_client.py <ip-esp8266> 7777 "ping"
```

## Documentation

- [Documentation de reference](docs/tcp_transport_esp8266.fr.md)
- [Modele de rapport memoire](docs/tcp_transport_memory_report.md)
- [Plan de test](tests/tcp_transport_test_plan.md)

## Licence

Aucune licence n'est encore definie.
