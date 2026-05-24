# Documentation de reference - Transport TCP ESP8266 RTOS SDK

## Objet

`esp8266-tcp-transport` fournit une brique de transport TCP serveur pour ESP8266 RTOS SDK.

Le module transporte des flux d'octets TCP. Il ne connait pas HTTP, les routes, les sessions, les contenus web ou la logique metier.

Objectifs principaux :

- fournir un serveur TCP minimal et reutilisable ;
- borner la consommation RAM ;
- eviter le modele une task par client ;
- masquer lwIP aux couches applicatives ;
- exposer une API C simple.

## Architecture

Pile logique cible :

```text
lwIP / FreeRTOS
    |
tcp_transport
    |
couche protocolaire optionnelle
    |
application
```

Le transport utilise une seule task FreeRTOS interne. Cette task surveille le socket serveur et les sockets clients avec `select()`.

Les sockets sont non bloquants. Les connexions clientes sont stockees dans des slots statiques. Les evenements clients sont traites sequentiellement dans la task reseau.

## Perimetre V1

Inclus :

- serveur TCP uniquement ;
- sockets IPv4 TCP non bloquants ;
- une task FreeRTOS reseau ;
- boucle `select()` ;
- slots clients statiques ;
- buffers RX/TX statiques par client ;
- callbacks globaux ;
- emission non bloquante bufferisee ;
- fermeture immediate ou apres vidage du TX ;
- timeout d'inactivite configurable ;
- logs de demarrage, arret, connexion, rejet, erreur, fermeture et saturation TX.

Hors perimetre :

- HTTP ;
- TLS ;
- WebSocket ;
- UDP ;
- IPv6 ;
- DNS ;
- authentification ;
- gestion de session ;
- queue applicative ;
- appels cross-task garantis ;
- allocation dynamique par client.

## Configuration publique

Les constantes principales sont definies dans `include/tcp_transport.h`.

| Symbole | Valeur par defaut | Role |
|---|---:|---|
| `TCP_SERVER_MAX_CLIENTS` | `3` | Nombre maximal de slots clients statiques |
| `TCP_RX_BUFFER_SIZE` | `512` | Taille du buffer de reception par client |
| `TCP_TX_BUFFER_SIZE` | `512` | Taille du buffer d'emission par client |
| `TCP_SELECT_TIMEOUT_MS` | `100` | Timeout de reveil de la boucle `select()` |
| `TCP_IDLE_TIMEOUT_MS` | `5000` | Timeout d'inactivite client, `0` pour desactiver |

Les constantes internes surchargeables dans `src/tcp_transport.c` :

| Symbole | Valeur par defaut | Role |
|---|---:|---|
| `TCP_NETWORK_TASK_STACK_SIZE` | `1024` | Stack de la task reseau, en mots FreeRTOS |
| `TCP_NETWORK_TASK_PRIORITY` | `5` | Priorite de la task reseau |
| `TCP_LISTEN_BACKLOG` | `TCP_SERVER_MAX_CLIENTS` | Backlog passe a `listen()` |
| `TCP_TRANSPORT_STACK_CHECK` | `0` | Active le log du watermark bas de stack |
| `TCP_ENABLE_REUSEADDR` | selon SDK | Active `SO_REUSEADDR` si disponible |

Toute modification de buffer, stack ou nombre de clients doit etre mesuree sur cible.

## Modele memoire

Le serveur utilise un etat global statique :

- un descripteur de socket serveur ;
- les callbacks globaux ;
- le handle de task reseau ;
- un tableau de `TCP_SERVER_MAX_CLIENTS` slots `tcp_conn_t`.

Chaque slot client contient :

- le fd client ;
- un buffer RX ;
- un buffer TX ;
- les longueurs et offsets TX/RX ;
- le timestamp de derniere activite ;
- l'etat du slot ;
- le flag `close_after_drain`.

Avec les valeurs par defaut, les buffers representent environ 3 KB pour 3 clients, hors structure de slot, stack FreeRTOS et memoire interne lwIP.

Les mesures reelles doivent etre documentees dans `docs/tcp_transport_memory_report.md`.

## Cycle de vie d'une connexion

| Etat | Signification |
|---|---|
| `TCP_SLOT_FREE` | Slot disponible |
| `TCP_SLOT_USED` | Connexion active |
| `TCP_SLOT_CLOSING` | Fermeture en cours |
| `TCP_SLOT_ERROR` | Erreur detectee avant fermeture |

Flux nominal :

1. `accept()` cree un fd client.
2. Le fd est passe en non bloquant.
3. Un slot libre est initialise.
4. `on_connect` est appele si defini.
5. Les receptions appellent `on_data`.
6. Les donnees TX sont envoyees progressivement.
7. La fermeture appelle `on_close` une seule fois.
8. Le slot revient a `TCP_SLOT_FREE`.

Si aucun slot n'est disponible, le client est accepte puis ferme immediatement. Le backlog TCP/lwIP n'est pas utilise comme mecanisme de regulation applicative.

## Contrat des callbacks

```c
typedef struct {
    void (*on_connect)(tcp_conn_t *conn);
    void (*on_data)(tcp_conn_t *conn, const uint8_t *buf, size_t len);
    void (*on_close)(tcp_conn_t *conn);
    void (*on_error)(tcp_conn_t *conn, int err);
} tcp_server_callbacks_t;
```

Regles :

- les callbacks s'executent dans la task reseau interne ;
- ils ne doivent pas bloquer ;
- ils ne doivent pas attendre une ressource lente ;
- ils ne doivent pas contenir de traitement long ;
- ils ne doivent pas exposer lwIP a la couche applicative.

`on_error` est suivi d'une fermeture de connexion et de `on_close`.

## Recueil des fonctions

### `tcp_server_start`

```c
int tcp_server_start(uint16_t port, uint8_t max_clients,
                     const tcp_server_callbacks_t *callbacks);
```

Demarre le serveur TCP.

Parametres :

- `port` : port TCP local. `0` est invalide.
- `max_clients` : nombre de clients autorises a l'execution. Doit etre compris entre `1` et `TCP_SERVER_MAX_CLIENTS`.
- `callbacks` : pointeur optionnel vers les callbacks applicatifs. `NULL` est accepte.

Comportement :

- initialise les slots ;
- cree le socket serveur ;
- bind sur `INADDR_ANY:port` ;
- passe le socket serveur en non bloquant ;
- appelle `listen()` ;
- demarre la task reseau interne.

Retours :

| Code | Signification |
|---|---|
| `TCP_TRANSPORT_OK` | Serveur demarre |
| `TCP_TRANSPORT_ERR_INVALID_ARG` | Port ou limite client invalide |
| `TCP_TRANSPORT_ERR_ALREADY_STARTED` | Serveur deja demarre |
| `TCP_TRANSPORT_ERR_SOCKET` | Echec de creation socket |
| `TCP_TRANSPORT_ERR_BIND` | Echec de `bind()` |
| `TCP_TRANSPORT_ERR_LISTEN` | Echec de `listen()` |
| `TCP_TRANSPORT_ERR_NONBLOCK` | Echec du passage non bloquant |
| `TCP_TRANSPORT_ERR_TASK` | Echec de creation de la task |

### `tcp_server_stop`

```c
int tcp_server_stop(void);
```

Demande l'arret du serveur TCP.

Comportement :

- met fin a la boucle reseau ;
- ferme tous les clients actifs ;
- ferme le socket serveur ;
- remet l'etat interne dans un etat redemarrable.

Si la fonction est appelee depuis la task reseau, elle demande l'arret et retourne sans attendre la suppression de la task.

Retours :

| Code | Signification |
|---|---|
| `TCP_TRANSPORT_OK` | Serveur arrete ou deja arrete |
| `TCP_TRANSPORT_ERR_STOP_TIMEOUT` | La task reseau n'a pas confirme l'arret dans le delai attendu |

### `tcp_send`

```c
size_t tcp_send(tcp_conn_t *conn, const uint8_t *buf, size_t len);
```

Copie des octets dans le buffer TX de la connexion.

Parametres :

- `conn` : connexion active appartenant au serveur.
- `buf` : donnees a envoyer.
- `len` : nombre d'octets demandes.

Comportement :

- ne bloque pas ;
- accepte uniquement l'espace disponible dans `tx_buf` ;
- retourne le nombre d'octets acceptes ;
- compacte le buffer TX si des octets precedents ont deja ete transmis ;
- refuse les nouveaux envois apres `tcp_close_after_drain()`.

Retour :

- `0` si l'appel est invalide, si le buffer est plein ou si aucun octet ne peut etre accepte ;
- `1..len` selon l'espace disponible.

Contrainte V1 : appelable uniquement depuis la task reseau interne.

### `tcp_close_after_drain`

```c
int tcp_close_after_drain(tcp_conn_t *conn);
```

Demande une fermeture serveur apres emission complete des octets deja acceptes dans le buffer TX.

Comportement :

- si le TX est vide, ferme immediatement ;
- si le TX contient des donnees, pose `close_after_drain` ;
- refuse ensuite tout nouvel envoi via `tcp_send()` ;
- ferme automatiquement quand `tx_offset == tx_len`.

Retours :

| Code | Signification |
|---|---|
| `TCP_TRANSPORT_OK` | Demande acceptee |
| `TCP_TRANSPORT_ERR_INVALID_ARG` | Connexion invalide ou appel hors task reseau |

Contrainte V1 : appelable uniquement depuis la task reseau interne.

### `tcp_transport_close`

```c
void tcp_transport_close(tcp_conn_t *conn);
```

Ferme immediatement une connexion active et libere son slot.

Comportement :

- ferme le fd client ;
- appelle `on_close` si applicable ;
- remet le slot a `TCP_SLOT_FREE`.

Les appels invalides sont ignores.

Contrainte V1 : appelable uniquement depuis la task reseau interne.

### `tcp_close`

```c
static inline void tcp_close(tcp_conn_t *conn);
```

Alias public inline de `tcp_transport_close()`.

Utiliser `tcp_close()` dans le code applicatif. `tcp_transport_close()` reste expose pour conserver un symbole C explicite.

## Codes de retour

```c
typedef enum {
    TCP_TRANSPORT_OK = 0,
    TCP_TRANSPORT_ERR_INVALID_ARG = -1,
    TCP_TRANSPORT_ERR_ALREADY_STARTED = -2,
    TCP_TRANSPORT_ERR_SOCKET = -3,
    TCP_TRANSPORT_ERR_BIND = -4,
    TCP_TRANSPORT_ERR_LISTEN = -5,
    TCP_TRANSPORT_ERR_NONBLOCK = -6,
    TCP_TRANSPORT_ERR_TASK = -7,
    TCP_TRANSPORT_ERR_STOP_TIMEOUT = -8
} tcp_transport_result_t;
```

## Exemple minimal

```c
static void on_data(tcp_conn_t *conn, const uint8_t *buf, size_t len)
{
    (void)tcp_send(conn, buf, len);
}

static tcp_server_callbacks_t callbacks = {
    .on_data = on_data,
};

void app_start_tcp(void)
{
    int ret = tcp_server_start(7777, TCP_SERVER_MAX_CLIENTS, &callbacks);
    if (ret != TCP_TRANSPORT_OK) {
        /* handle startup error */
    }
}
```

## Validation

La validation minimale attendue :

- compilation PlatformIO ;
- demarrage serveur ;
- connexion d'un client ;
- reception de donnees ;
- emission via `tcp_send()`;
- fermeture distante ;
- saturation TX ;
- rejet du client excedentaire ;
- arret serveur ;
- redemarrage serveur ;
- mesure heap et stack sur cible.

Le plan de test detaille est dans `tests/tcp_transport_test_plan.md`.

## Limites connues

- Pas d'API cross-task garantie en V1.
- Pas de queue applicative.
- Pas de backpressure avancee.
- Les callbacks longs retardent tous les clients.
- Les mesures memoire cible restent a renseigner dans le rapport dedie.
