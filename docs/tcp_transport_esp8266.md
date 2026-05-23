# Transport TCP — ESP8266 RTOS SDK

## Objectif réel

Ne pas créer directement un serveur web ESP8266 RTOS SDK.
Avant http_server, il faut décider si le socle doit inclure une abstraction transport générique.

## Décision d'architecture

Le serveur web ne doit pas être la première brique réseau.
La bonne pile logique est :

```
lwIP / FreeRTOS
    ↓
transport générique TCP
    ↓
HTTP minimal
    ↓
services web/API
    ↓
logique métier projet
```

## Choix de transport

Options évaluées :

| Option | Verdict | Raison |
|---|---|---|
| Sockets bloquants + task dédiée | Écarté | 1 task/client = 3×stack FreeRTOS ≈ 6–9 KB RAM, non scalable |
| lwIP netconn | Écarté | Mailbox par connexion, blocage interne, overhead caché |
| lwIP raw API | Écarté | Callbacks dans le thread lwIP, risque élevé, debugging douloureux |
| **Sockets non-bloquants + select** | **Retenu** | Lisible, portable, 1 seule task, coût mémoire prévisible |

## Architecture retenue

Une task FreeRTOS dédiée exécute une boucle `select()` sur le socket serveur et les sockets clients actifs.
Les événements clients sont traités séquentiellement dans cette task.
Il n'y a pas de concurrence applicative entre clients dans la couche transport.

## Concurrence

`TCP_SERVER_MAX_CLIENTS = 3` fixe l'allocation statique à la compilation.
La valeur runtime `max_clients` passée à l'init doit être `<= TCP_SERVER_MAX_CLIENTS`.
Chaque client occupe un slot statique.

Dimensionnement RAM :
- buffer RX par slot : 256–512 octets
- buffer TX par slot : 256–512 octets
- état par slot : fd, rx_len, tx_len, tx_offset, timestamp, state (~64 octets)
- total slots 3 clients : ~3 KB réservés statiquement
- stack task réseau : 2–4 KB selon usage, à définir
- structures lwIP internes : hors contrôle direct
- **total applicatif minimal : ~5–7 KB avant HTTP**

## Rejet

La bibliothèque ne maintient aucune queue applicative.
Le rejet est applicatif : la bibliothèque accepte puis ferme immédiatement le fd au-delà du seuil.
Le backlog TCP/lwIP existe toujours, mais il n'est pas utilisé comme mécanisme de régulation applicative.

## État connexion

Lifecycle du slot — quatre états mutuellement exclusifs :

```c
typedef enum {
    TCP_SLOT_FREE = 0,   /* slot disponible */
    TCP_SLOT_USED,       /* connexion active */
    TCP_SLOT_CLOSING,    /* fermeture en cours */
    TCP_SLOT_ERROR       /* erreur, implique fermeture */
} tcp_slot_state_t;
```

Logique buffer TX :
```
tx_offset < tx_len  → il reste des octets à envoyer
tx_offset == tx_len → buffer TX vide, reset tx_len/tx_offset à 0
```

`tcp_send()` bufferise dans `tx_buf`, ne bloque pas, retourne le nombre d'octets acceptés.
Les envois partiels sont repris à `tx_offset` sur le prochain passage dans la boucle `select()`.

## Fermeture après vidage TX

Le transport expose `tcp_close_after_drain()` pour permettre à une couche supérieure de demander la fermeture serveur après émission complète des données déjà bufferisées.

Si le buffer TX est vide, la fermeture est immédiate.
Si le buffer TX contient encore des données, le transport poursuit les envois partiels puis ferme automatiquement la connexion lorsque `tx_offset == tx_len`.

Après appel à `tcp_close_after_drain()`, aucun nouvel envoi applicatif n'est accepté sur cette connexion : `tcp_send()` retourne `0`.

## Thread-safety

Les callbacks tournent dans la task réseau.
`tcp_send()`, `tcp_close_after_drain()` et `tcp_close()` sont appelables uniquement depuis cette task en V1.
Les appels cross-task sont hors V1 — ils passeront par une queue de commandes FreeRTOS si le besoin apparaît.

## Contrat transport (interface minimale)

Pas de vtable générique. Pas de polymorphisme dynamique.
Séparation état par connexion / callbacks globaux :

```c
/* état par connexion — 1 slot par client */
typedef struct {
    int      fd;

    uint8_t  rx_buf[512];
    size_t   rx_len;

    uint8_t  tx_buf[512];
    size_t   tx_len;
    size_t   tx_offset;

    uint32_t last_activity_ms;
    tcp_slot_state_t state;
    bool close_after_drain;
} tcp_conn_t;

/* callbacks globaux — définis une fois à l'init */
typedef struct {
    void (*on_connect)(tcp_conn_t *conn);
    void (*on_data)   (tcp_conn_t *conn, const uint8_t *buf, size_t len);
    void (*on_close)  (tcp_conn_t *conn);
    void (*on_error)  (tcp_conn_t *conn, int err);
} tcp_server_callbacks_t;
```

Opérations exposées :
- `tcp_server_start(port, max_clients, callbacks)` — lie, écoute, configure non-bloquant, démarre la task interne
- `tcp_server_stop()` — arrête la task, ferme tous les fds, libère les slots, retourne le statut d'arrêt
- `tcp_send(tcp_conn_t *conn, buf, len)` — bufferise, envoi non-bloquant, retourne octets acceptés
- `tcp_close_after_drain(tcp_conn_t *conn)` — ferme après vidage complet du TX déjà accepté
- `tcp_close(tcp_conn_t *conn)` — fermeture propre, libère le slot

Coût mémoire du module : à mesurer après compilation avec les flags projet.

## Règle callback

Aucun callback ne doit bloquer.
Aucun traitement long dans la couche transport.
Le transport notifie, pousse les octets, ferme proprement, et reste minimal.

## Règle de conception

`http_server` ne doit pas exposer lwIP directement.
Il doit dépendre du contrat transport ci-dessus : stable, borné RAM, réutilisable.

## Finalité

Obtenir des briques personnelles cohérentes entre projets :
- même modèle réseau
- mêmes limites mémoire
- mêmes timeouts
- mêmes logs
- mêmes erreurs
- même comportement de service

## Conclusion

Le transport est cadré :
- sockets non-bloquants + select, task FreeRTOS interne
- `TCP_SERVER_MAX_CLIENTS = 3` statique, `max_clients` runtime borné
- slots clients statiques, lifecycle explicite : `FREE` / `USED` / `CLOSING` / `ERROR`
- état par connexion séparé des callbacks globaux
- buffers RX/TX bornés, envois partiels gérés par `tx_len` + `tx_offset`
- rejet applicatif : `accept()` puis `close()` immédiat au-delà du seuil
- backlog TCP/lwIP reconnu mais non utilisé comme régulation applicative
- thread-safety V1 : tout dans la task réseau
- coût mémoire à mesurer : slots (~3 KB) + stack task (2–4 KB) + overhead lwIP
- contrat minimal en C pur, prêt à servir de base à `http_server`
