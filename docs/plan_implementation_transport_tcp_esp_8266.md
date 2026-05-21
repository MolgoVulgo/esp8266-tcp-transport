# Plan d’implémentation — Transport TCP ESP8266 RTOS SDK

## 1. Objectif

Implémenter une brique TCP serveur minimale pour ESP8266 RTOS SDK.

Le module doit fournir un socle réseau générique avant toute couche HTTP, avec une consommation RAM bornée, une API C simple, une task réseau unique et une gestion explicite des connexions.

## 1.1 Références de cadrage

Ce plan applique les documents suivants :

- `agents.md` : contraintes de travail, bornage ESP8266, règles réseau, mémoire, FreeRTOS et callbacks ;
- `docs/cahier_fonctionnel_transport_tcp_esp_8266.md` : besoin fonctionnel, périmètre V1, critères d’acceptation ;
- `docs/tcp_transport_esp8266.md` : décision d’architecture et contrat transport minimal.

En cas d’écart entre ce plan et le cahier fonctionnel, le cahier fonctionnel prévaut. En cas d’écart avec `agents.md`, `agents.md` prévaut.

## 2. Principe directeur

La V1 ne cherche pas à être générique au sens large.

Elle doit être :

- simple ;
- mesurable ;
- stable ;
- testable ;
- indépendante de HTTP ;
- sans allocation dynamique par client ;
- sans exposition directe de lwIP aux couches supérieures.

La priorité est la maîtrise du comportement, pas l’abstraction prématurée.

## 2.1 Garde-fous d’implémentation

Pendant l’implémentation, chaque phase doit rester vérifiable isolément.

Règles à maintenir pendant toute la V1 :

- pas de serveur HTTP dans ce module ;
- pas de TLS, WebSocket, UDP, IPv6, DNS, authentification ou session ;
- pas d’allocation dynamique par client ;
- pas de queue FreeRTOS applicative ;
- pas de modèle une task par client ;
- pas d’exposition directe des sockets lwIP aux couches applicatives ;
- pas d’appel cross-task garanti pour `tcp_send()` ou `tcp_close()` ;
- callbacks courts, non bloquants, exécutés depuis la task réseau.

## 3. Architecture cible

Pile cible :

```text
lwIP / FreeRTOS
    ↓
tcp_transport
    ↓
http_server minimal
    ↓
services web / API
    ↓
logique métier projet
```

Le module `tcp_transport` est responsable uniquement du transport d’octets TCP.

Il ne connaît pas HTTP, les routes, les sessions, les contenus web ou la logique métier.

## 4. Organisation proposée des fichiers

Structure minimale recommandée :

```text
include/
└── tcp_transport.h

src/
└── tcp_transport.c

tests/
└── tcp_transport_test_plan.md

docs/
└── tcp_transport_memory_report.md
```

### `include/tcp_transport.h`

Contient :

- constantes publiques ;
- types publics ;
- enum d’état des slots ;
- structure `tcp_conn_t` ;
- structure `tcp_server_callbacks_t` ;
- prototypes de l’API publique.

### `src/tcp_transport.c`

Contient :

- état interne serveur ;
- tableau statique des slots ;
- création socket serveur ;
- passage en non-bloquant ;
- task FreeRTOS réseau ;
- boucle `select()` ;
- acceptation clients ;
- lecture RX ;
- écriture TX ;
- fermeture et nettoyage ;
- logs internes.

### `tests/tcp_transport_test_plan.md`

Contient les scénarios de validation manuelle ou semi-automatisée.

### `docs/tcp_transport_memory_report.md`

Contient les mesures mémoire réelles après compilation et test sur cible.

## 5. Phase 0 — Préparation du module

### Objectif

Créer le squelette propre du module sans logique réseau complète.

### Actions

1. Créer les fichiers `tcp_transport.h` et `tcp_transport.c`.
2. Définir les constantes :

```c
#define TCP_SERVER_MAX_CLIENTS  3
#define TCP_RX_BUFFER_SIZE      512
#define TCP_TX_BUFFER_SIZE      512
#define TCP_SELECT_TIMEOUT_MS   100
```

3. Définir les états de slot :

```c
typedef enum {
    TCP_SLOT_FREE = 0,
    TCP_SLOT_USED,
    TCP_SLOT_CLOSING,
    TCP_SLOT_ERROR
} tcp_slot_state_t;
```

4. Définir la structure publique de connexion.
5. Définir la structure des callbacks.
6. Déclarer les fonctions publiques :

```c
tcp_server_start(...)
tcp_server_stop(...)
tcp_send(...)
tcp_close(...)
```

### Critères de sortie

- Le module compile sans logique active.
- Les types publics sont disponibles.
- Aucune dépendance HTTP n’existe.
- Aucun client n’est encore accepté.

## 6. Phase 1 — État interne serveur

### Objectif

Mettre en place les structures internes nécessaires à la gestion du serveur et des slots.

### Actions

1. Créer une structure interne serveur contenant :

- fd serveur ;
- port ;
- `max_clients` runtime ;
- callbacks ;
- handle de task réseau ;
- flag `running` ;
- tableau statique de `tcp_conn_t`.

2. Initialiser tous les slots à `TCP_SLOT_FREE`.
3. Ajouter des helpers internes :

- trouver un slot libre ;
- compter les slots utilisés ;
- fermer un slot ;
- réinitialiser un slot ;
- valider qu’une connexion appartient au tableau statique.

### Critères de sortie

- Les slots sont initialisables et réinitialisables.
- `max_clients` est borné par `TCP_SERVER_MAX_CLIENTS`.
- Le module refuse une configuration invalide.

## 7. Phase 2 — Démarrage serveur TCP

### Objectif

Implémenter `tcp_server_start()` jusqu’à l’écoute TCP active.

### Actions

1. Vérifier les paramètres :

- port valide ;
- `max_clients > 0` ;
- `max_clients <= TCP_SERVER_MAX_CLIENTS` ;
- callbacks valides ou explicitement optionnels selon règle retenue.

2. Créer le socket serveur.
3. Activer `SO_REUSEADDR` si disponible.
4. Binder sur le port demandé.
5. Passer le socket en non-bloquant.
6. Appeler `listen()`.
7. Initialiser l’état interne.
8. Créer la task réseau FreeRTOS.

### Critères de sortie

- Le serveur écoute sur le port demandé.
- Le socket serveur est non-bloquant.
- La task réseau démarre.
- Les erreurs de démarrage sont propres et loguées.
- En cas d’échec partiel, les fd déjà ouverts sont fermés.

## 8. Phase 3 — Task réseau et boucle `select()`

### Objectif

Créer la boucle centrale du transport.

### Mécanisme de réveil au stop

Le réveil de `select()` repose sur un timeout court fixe. `pipe()` n'est pas garanti sur ESP8266 RTOS SDK et n'est pas retenu.

```c
struct timeval tv;
tv.tv_sec  = 0;
tv.tv_usec = TCP_SELECT_TIMEOUT_MS * 1000;

while (running) {
    FD_ZERO(&rfds);
    /* ... ajout des fd ... */

    int ret = select(max_fd + 1, &rfds, &wfds, NULL, &tv);

    if (!running) break;

    if (ret < 0)  { /* erreur select */ }
    if (ret == 0) { /* timeout — vérifier inactivité clients */ continue; }

    /* traitement événements */
}
```

Latence maximale à l'arrêt : 100ms. Acceptable sur ce chemin non critique.

### Actions

1. Construire un `fd_set` à chaque itération.
2. Ajouter le fd serveur.
3. Ajouter les fd clients actifs.
4. Calculer `max_fd`.
5. Appeler `select()` avec timeout court.
6. Traiter les événements dans cet ordre :

```text
1. nouvelle connexion serveur
2. réception clients
3. émission clients
4. fermeture différée / erreur
```

7. Maintenir une boucle tant que `running == true`.
8. Nettoyer tous les sockets avant sortie de task.

### Critères de sortie

- La task ne bloque pas indéfiniment.
- `tcp_server_stop()` peut interrompre proprement la boucle.
- Aucun callback n’est appelé hors task réseau.
- Les clients sont traités séquentiellement.

## 9. Phase 4 — Acceptation clients

### Objectif

Accepter les connexions entrantes en respectant la limite statique.

### Actions

1. Lorsqu’un événement arrive sur le socket serveur, appeler `accept()`.
2. Passer le client en non-bloquant.
3. Vérifier le nombre de clients actifs.
4. Si un slot est disponible et autorisé :

- affecter le fd au slot ;
- initialiser les buffers ;
- passer l’état à `TCP_SLOT_USED` ;
- renseigner `last_activity_ms` ;
- appeler `on_connect`.

5. Si aucun slot n’est disponible :

- accepter le fd ;
- logger le rejet ;
- fermer immédiatement le fd.

### Critères de sortie

- Jusqu’à 3 clients peuvent être gérés selon configuration.
- Un client supplémentaire est rejeté proprement.
- Le backlog TCP n’est pas utilisé comme régulation applicative.
- Chaque client accepté correspond à un slot statique.

## 10. Phase 5 — Réception RX

### Objectif

Lire les données disponibles sans blocage et notifier l’application.

### Actions

1. Sur fd client lisible, appeler `recv()`.
2. Si `recv() > 0` :

- stocker les octets dans `rx_buf` ;
- renseigner `rx_len` ;
- mettre à jour `last_activity_ms` ;
- appeler `on_data(conn, rx_buf, rx_len)`.

3. Si `recv() == 0` :

- considérer que le client a fermé ;
- passer en fermeture contrôlée.

4. Si `recv() < 0` :

- ignorer `EWOULDBLOCK` / `EAGAIN` ;
- passer en erreur pour les autres cas ;
- appeler `on_error` si défini.

### Critères de sortie

- Les données clientes arrivent bien dans `on_data`.
- Aucune queue applicative n’est créée.
- Le callback reçoit uniquement les données disponibles.
- Les déconnexions distantes sont détectées.

## 11. Phase 6 — Buffer TX et `tcp_send()`

### Objectif

Implémenter un envoi non bloquant basé sur buffer TX statique.

### Actions

1. Vérifier que `conn` est valide.
2. Vérifier que le slot est en état `TCP_SLOT_USED`.
3. Calculer l’espace disponible dans `tx_buf`.
4. Copier uniquement les octets acceptables.
5. Mettre à jour `tx_len`.
6. Retourner le nombre d’octets acceptés.
7. Ne jamais bloquer.
8. Ne jamais appeler directement une boucle d’envoi longue depuis `tcp_send()`.

### Critères de sortie

- `tcp_send()` retourne un nombre d’octets accepté.
- La saturation TX est visible par retour partiel ou zéro.
- Aucun dépassement de buffer n’est possible.
- L’appel est valide uniquement depuis la task réseau en V1.

## 12. Phase 7 — Émission TX dans la boucle réseau

### Objectif

Envoyer progressivement les données présentes dans le buffer TX.

### Actions

1. Pour chaque client avec `tx_offset < tx_len`, tenter `send()`.
2. Si `send() > 0` :

- avancer `tx_offset` ;
- mettre à jour `last_activity_ms`.

3. Si `tx_offset == tx_len` :

- remettre `tx_len = 0` ;
- remettre `tx_offset = 0`.

4. Si `send() < 0` :

- ignorer `EWOULDBLOCK` / `EAGAIN` ;
- passer en erreur pour les autres cas.

### Critères de sortie

- Les envois partiels reprennent correctement.
- Le buffer TX se vide progressivement.
- Aucune attente active n’est introduite.
- Une erreur d’envoi ferme proprement la connexion.

## 13. Phase 8 — Fermeture connexion

### Objectif

Implémenter `tcp_close()` et la fermeture interne des slots.

### Actions

1. Vérifier que `conn` est valide.
2. Passer l’état à `TCP_SLOT_CLOSING`.
3. Fermer le fd si ouvert.
4. Appeler `on_close` si applicable.
5. Réinitialiser fd, buffers, longueurs, timestamps.
6. Repasser le slot à `TCP_SLOT_FREE`.
7. Protéger contre les doubles fermetures.

### Critères de sortie

- Un slot fermé est réutilisable.
- Une fermeture distante, locale ou sur erreur suit le même chemin de nettoyage.
- Aucun fd fermé ne reste dans `select()`.

## 14. Phase 9 — Arrêt serveur

### Objectif

Implémenter `tcp_server_stop()`.

### Actions

1. Mettre `running = false`.
2. Forcer le réveil de la boucle `select()` si nécessaire.
3. Fermer le socket serveur.
4. Fermer tous les clients actifs.
5. Attendre ou laisser sortir la task selon modèle FreeRTOS retenu.
6. Réinitialiser l’état global serveur.

### Critères de sortie

- Le serveur peut être arrêté sans fuite de fd.
- Tous les slots reviennent à `TCP_SLOT_FREE`.
- Le module peut être redémarré après arrêt.

## 15. Phase 10 — Timeouts d’inactivité

### Objectif

Préparer la gestion des clients silencieux.

### Note de conception

Le timeout d’inactivité s’appuie sur le même réveil périodique que le mécanisme d’arrêt : `select()` se réveille toutes les `TCP_SELECT_TIMEOUT_MS` ms. Le cas `ret == 0` est le point naturel pour vérifier l’inactivité des clients. Deux problèmes résolus par la même mécanique, sans coût supplémentaire.

### Actions

1. Mettre à jour `last_activity_ms` à chaque RX ou TX réussi.
2. Ajouter une constante de timeout par défaut.
3. À chaque tour de boucle, comparer l’activité des clients.
4. Fermer les connexions trop anciennes.
5. Logger la fermeture sur timeout.

### Critères de sortie

- Un client inactif ne reste pas connecté indéfiniment.
- Le comportement est homogène entre projets.
- Le champ `last_activity_ms` est réellement exploité.

## 16. Phase 11 — Logs et diagnostic

### Objectif

Rendre le module observable sans le rendre bavard.

### Logs minimaux

- démarrage serveur ;
- port écouté ;
- `max_clients` effectif ;
- acceptation client ;
- rejet client ;
- fermeture client ;
- erreur socket ;
- saturation TX ;
- arrêt serveur ;
- mémoire libre si API disponible.

### Critères de sortie

- Les problèmes réseau sont diagnostiquables.
- Les logs ne saturent pas la console.
- Les logs restent désactivables ou abaissables selon niveau projet.

## 17. Phase 12 — Mesures mémoire

### Objectif

Valider le coût réel du module sur cible.

### Mesures attendues

Documenter dans `docs/tcp_transport_memory_report.md` :

- taille firmware avant/après ajout module ;
- RAM statique consommée ;
- stack task réseau configurée ;
- stack réellement utilisée si mesurable ;
- RAM libre serveur arrêté ;
- RAM libre serveur démarré sans client ;
- RAM libre avec 1 client ;
- RAM libre avec 3 clients ;
- comportement en saturation TX ;
- comportement en saturation clients.

### Critères de sortie

- La consommation réelle est connue.
- Les tailles RX/TX peuvent être arbitrées sur mesure réelle.
- Le module est validé avant ajout de HTTP.

## 18. Phase 13 — Tests fonctionnels

### Scénarios à valider

1. Démarrage serveur sur port valide.
2. Échec propre sur port invalide ou bind impossible.
3. Connexion d’un client unique.
4. Connexion de trois clients.
5. Rejet du quatrième client.
6. Réception de données simples.
7. Réception de plusieurs paquets successifs.
8. Envoi simple via `tcp_send()`.
9. Envoi partiel avec reprise `tx_offset`.
10. Saturation du buffer TX.
11. Fermeture côté client.
12. Fermeture côté serveur.
13. Déconnexion brutale client.
14. Arrêt serveur avec clients actifs.
15. Redémarrage serveur après arrêt.

### Outils de test possibles

- `nc` / `netcat` ;
- `telnet` ;
- script Python TCP local ;
- logs série ESP8266 ;
- mesures heap ESP8266 RTOS SDK.

## 19. Phase 14 — Stabilisation API

### Objectif

Figer le contrat avant développement HTTP.

### Actions

1. Vérifier que l’API publique ne contient aucune notion HTTP.
2. Vérifier que `tcp_conn_t` expose uniquement l’état utile.
3. Vérifier que les fonctions publiques sont suffisantes pour HTTP minimal.
4. Vérifier que les contraintes V1 sont documentées.
5. Vérifier que les appels cross-task sont explicitement interdits.

### Critères de sortie

- Le contrat TCP est stable.
- Le futur HTTP peut s’appuyer dessus sans accéder à lwIP.
- Les limites V1 sont claires.

## 20. Ordre d’implémentation recommandé

Ordre strict :

```text
1. types publics + constantes
2. état interne serveur
3. helpers slots
4. start serveur
5. task réseau vide
6. select serveur
7. accept client
8. RX client
9. close client
10. stop serveur
11. tcp_send bufferisé
12. TX progressif
13. erreurs socket
14. timeout activité
15. logs
16. mesures mémoire
17. tests saturation
18. stabilisation API
```

Ne pas commencer HTTP avant validation complète de l’étape 18.

## 21. Règles de non-régression

À chaque ajout de phase, vérifier :

- compilation propre ;
- aucun warning critique ;
- serveur toujours démarrable ;
- arrêt propre ;
- pas de fuite de slot ;
- pas de fd orphelin ;
- pas d’allocation dynamique par client ;
- pas de dépendance HTTP ;
- pas d’appel callback hors task réseau ;
- RAM libre cohérente.

## 22. Risques techniques

| Risque | Impact | Réponse |
|---|---|---|
| Stack task trop faible | crash ou comportement instable | mesurer high watermark |
| Buffers 512 trop coûteux | RAM trop basse | prévoir variante 256 |
| `select()` mal réveillé au stop | arrêt bloqué | timeout fixe `TCP_SELECT_TIMEOUT_MS = 100ms` retenu — `pipe()` non garanti sur ESP8266 RTOS SDK |
| callbacks trop longs | latence clients | règle stricte : callback court |
| saturation TX | pertes applicatives possibles | retour partiel explicite |
| appels cross-task accidentels | corruption ou comportement instable | documenter et valider contexte V1 |
| HTTP ajouté trop tôt | couplage prématuré | valider TCP seul d’abord |

## 23. Arbitrages à faire après V1 fonctionnelle

À décider uniquement après mesures :

- RX/TX à 256 ou 512 octets ;
- stack task exacte ;
- timeout d’inactivité par défaut (mécanique de réveil tranchée : `TCP_SELECT_TIMEOUT_MS = 100ms`) ;
- niveau de logs permanent ;
- politique de retour d’erreur ;
- ajout ou non d’une queue de commandes cross-task ;
- adaptation éventuelle à d’autres projets ESP.

## 24. Définition de terminé

Le plan est terminé lorsque :

- le serveur TCP démarre et s’arrête proprement ;
- 3 clients maximum sont gérés via slots statiques ;
- le 4e client est rejeté proprement ;
- RX fonctionne via callback ;
- TX fonctionne par buffer non bloquant ;
- les envois partiels sont repris ;
- les fermetures libèrent les slots ;
- les erreurs socket sont traitées ;
- les timeouts sont opérationnels ou documentés comme désactivés ;
- les mesures mémoire sont produites ;
- aucun élément HTTP n’est présent ;
- le futur `http_server` peut utiliser uniquement l’API publique du transport.
