# Cahier fonctionnel — Transport TCP ESP8266 RTOS SDK

## 1. Objet du document

Ce cahier fonctionnel définit le besoin, le périmètre et les règles attendues pour une brique de transport TCP générique destinée à l’ESP8266 RTOS SDK.

L’objectif n’est pas de développer directement un serveur HTTP, mais de créer un socle TCP minimal, stable, borné en mémoire et réutilisable par plusieurs projets embarqués.

## 2. Contexte

Les projets ESP8266 disposent d’une RAM limitée. La mise en place directe d’un serveur web expose rapidement le firmware à des risques de dérive mémoire, de complexité réseau et de couplage fort avec lwIP.

Le transport TCP doit donc être isolé avant toute couche HTTP.

Pile logique retenue :

```text
lwIP / FreeRTOS
    ↓
transport générique TCP
    ↓
HTTP minimal
    ↓
services web / API
    ↓
logique métier projet
```

## 3. Objectifs fonctionnels

Le module doit fournir une couche TCP serveur générique permettant :

- d’accepter plusieurs connexions TCP clientes dans une limite fixe ;
- de recevoir des données depuis chaque client actif ;
- de transmettre des données sans blocage applicatif ;
- de fermer proprement une connexion ;
- de notifier l’application via callbacks ;
- de masquer lwIP aux couches supérieures ;
- de servir de base à un futur serveur HTTP minimal.

## 4. Objectifs d’architecture

Le module doit :

- rester indépendant du protocole applicatif ;
- ne pas dépendre directement d’un serveur HTTP ;
- garantir une consommation RAM prévisible ;
- éviter le modèle `1 task = 1 client` ;
- éviter les surcoûts cachés liés aux queues ou mailbox par connexion ;
- éviter les callbacks lwIP raw exécutés directement dans le thread lwIP ;
- fournir une API C simple et stable.

## 5. Périmètre V1

La V1 couvre uniquement :

- un serveur TCP ;
- une task réseau interne unique ;
- des sockets non-bloquants ;
- une boucle `select()` ;
- un nombre maximum de clients fixé à la compilation ;
- des slots clients statiques ;
- des buffers RX/TX statiques par client ;
- des callbacks globaux déclarés à l’initialisation ;
- l’envoi partiel par buffer TX ;
- la fermeture propre des connexions ;
- le rejet applicatif des connexions au-delà du seuil autorisé.

## 6. Hors périmètre V1

La V1 ne couvre pas :

- serveur HTTP complet ;
- parser HTTP ;
- TLS ;
- WebSocket ;
- UDP ;
- IPv6 ;
- DNS ;
- authentification ;
- gestion de sessions applicatives ;
- queue applicative ;
- appels cross-task vers `tcp_send()` ou `tcp_close()` ;
- allocation dynamique par client ;
- mécanisme de backpressure avancé ;
- priorisation entre clients.

## 7. Choix fonctionnel retenu

Le transport repose sur des sockets non-bloquants avec `select()`.

Ce choix est retenu car il fournit :

- un modèle lisible ;
- une seule task réseau ;
- une consommation mémoire bornée ;
- un comportement portable ;
- un débogage plus simple que lwIP raw API ;
- moins d’overhead que netconn ;
- une meilleure scalabilité RAM qu’un modèle task par client.

## 8. Modèle de concurrence

Le module fonctionne avec une seule task FreeRTOS dédiée au réseau.

Cette task :

- surveille le socket serveur ;
- surveille les sockets clients actifs ;
- accepte les nouveaux clients ;
- lit les données disponibles ;
- écrit les données en attente ;
- déclenche les callbacks ;
- ferme les connexions en erreur ou terminées.

Les événements clients sont traités séquentiellement dans cette task.

Il n’existe pas de concurrence applicative entre clients dans la couche transport V1.

## 9. Limite de connexions

La limite statique est définie par :

```text
TCP_SERVER_MAX_CLIENTS = 3
```

La valeur runtime `max_clients` passée à l’initialisation doit être inférieure ou égale à cette limite.

Chaque client accepté occupe un slot statique.

Si tous les slots autorisés sont occupés, la connexion est acceptée puis fermée immédiatement.

Le backlog TCP/lwIP peut exister, mais il ne constitue pas un mécanisme de régulation applicative.

## 10. Gestion mémoire attendue

Le module doit réserver statiquement les ressources nécessaires aux clients.

Estimation cible :

| Élément | Estimation |
|---|---:|
| Buffer RX par slot | 256 à 512 octets |
| Buffer TX par slot | 256 à 512 octets |
| État par slot | environ 64 octets |
| 3 slots clients | environ 3 KB |
| Stack task réseau | 2 à 4 KB |
| Total applicatif minimal avant HTTP | environ 5 à 7 KB |

Les structures internes lwIP sont hors contrôle direct du module et devront être observées en mesure réelle.

Le coût mémoire final doit être mesuré après compilation avec les flags du projet.

## 11. Lifecycle d’une connexion

Chaque slot client possède un état unique parmi :

| État | Signification |
|---|---|
| `TCP_SLOT_FREE` | slot disponible |
| `TCP_SLOT_USED` | connexion active |
| `TCP_SLOT_CLOSING` | fermeture en cours |
| `TCP_SLOT_ERROR` | erreur détectée, fermeture requise |

Règles attendues :

- un slot libre peut recevoir une nouvelle connexion ;
- un slot utilisé peut recevoir et envoyer des données ;
- un slot en fermeture ne doit plus accepter de nouvelles écritures applicatives ;
- un slot en erreur doit être fermé et libéré ;
- après fermeture, le slot revient à l’état libre.

## 12. Réception des données

Lorsqu’un client envoie des données :

- la task réseau lit les octets disponibles ;
- les données sont placées dans le buffer RX du slot ;
- le callback `on_data` est appelé ;
- le callback reçoit la connexion, le buffer et la longueur reçue ;
- le traitement applicatif doit rester court et non bloquant.

Le module ne conserve pas de queue applicative.

## 13. Émission des données

L’émission repose sur un buffer TX par connexion.

La fonction `tcp_send()` :

- ne bloque pas ;
- copie les octets acceptés dans le buffer TX ;
- retourne le nombre d’octets effectivement acceptés ;
- peut accepter moins d’octets que demandé si le buffer est partiellement occupé ;
- n’effectue pas de boucle d’attente active.

Règle de progression :

```text
tx_offset < tx_len  → données encore à envoyer
tx_offset == tx_len → buffer TX vide, reset tx_len/tx_offset à 0
```

Les envois partiels sont repris automatiquement lors des passages suivants dans la boucle `select()`.

## 14. Fermeture des connexions

La fonction `tcp_close()` doit permettre de fermer proprement une connexion active.
La fonction `tcp_close_after_drain()` doit permettre de demander une fermeture serveur après émission complète des octets déjà acceptés dans le buffer TX.

La fermeture doit :

- empêcher les écritures futures sur le slot ;
- fermer le descripteur de fichier ;
- appeler le callback de fermeture si applicable ;
- remettre le slot dans un état réutilisable ;
- éviter les doubles fermetures.

### Fermeture après vidage TX

Le transport expose `tcp_close_after_drain()` pour permettre à une couche supérieure de demander la fermeture serveur après émission complète des données déjà bufferisées.

Si le buffer TX est vide, la fermeture est immédiate.
Si le buffer TX contient encore des données, le transport poursuit les envois partiels puis ferme automatiquement la connexion lorsque `tx_offset == tx_len`.

Après appel à `tcp_close_after_drain()`, aucun nouvel envoi applicatif ne doit être accepté sur cette connexion.

Une erreur réseau doit conduire à l’état `TCP_SLOT_ERROR`, puis à une fermeture contrôlée.

## 15. Callbacks applicatifs

Le module expose quatre callbacks globaux déclarés à l’initialisation :

| Callback | Rôle |
|---|---|
| `on_connect` | notifier l’acceptation d’un client |
| `on_data` | notifier la réception de données |
| `on_close` | notifier la fermeture d’une connexion |
| `on_error` | notifier une erreur réseau ou slot |

Règles impératives :

- aucun callback ne doit bloquer ;
- aucun callback ne doit effectuer de traitement long ;
- aucun callback ne doit attendre une ressource lente ;
- les callbacks sont exécutés dans la task réseau ;
- les callbacks ne doivent pas créer de dépendance directe à lwIP.

## 16. API publique attendue

L’API publique minimale doit exposer :

| Fonction | Rôle fonctionnel |
|---|---|
| `tcp_server_start(port, max_clients, callbacks)` | démarrer le serveur TCP |
| `tcp_server_stop()` | arrêter le serveur et fermer les connexions, retourner le statut d'arrêt |
| `tcp_send(conn, buf, len)` | demander l’envoi non bloquant de données |
| `tcp_close_after_drain(conn)` | fermer après vidage complet du TX déjà accepté |
| `tcp_close(conn)` | demander la fermeture d’une connexion |

`tcp_server_start()` doit :

- créer le socket serveur ;
- binder sur le port demandé ;
- passer le socket en non-bloquant ;
- écouter les connexions entrantes ;
- initialiser les slots ;
- démarrer la task réseau interne.

`tcp_server_stop()` doit :

- arrêter la task réseau ;
- fermer le socket serveur ;
- fermer tous les sockets clients ;
- libérer ou réinitialiser les slots ;
- remettre le module dans un état redémarrable ;
- retourner `TCP_TRANSPORT_OK` ou `TCP_TRANSPORT_ERR_STOP_TIMEOUT`.

## 17. Thread-safety V1

La V1 impose que :

- `tcp_send()` soit appelée uniquement depuis la task réseau ;
- `tcp_close_after_drain()` soit appelée uniquement depuis la task réseau ;
- `tcp_close()` soit appelée uniquement depuis la task réseau ;
- les callbacks soient le point d’entrée normal vers ces fonctions ;
- aucun appel cross-task ne soit garanti.

Les appels depuis une autre task sont hors périmètre V1.

Une évolution future pourra ajouter une queue de commandes FreeRTOS si le besoin apparaît.

## 18. Timeouts et activité

Chaque connexion doit conserver un timestamp de dernière activité.

Ce timestamp sert à préparer :

- la détection des connexions inactives ;
- la fermeture des clients silencieux ;
- la cohérence future avec HTTP ;
- l’uniformisation des comportements entre projets.

Le timeout exact peut rester paramétrable ou défini ultérieurement, mais le champ d’activité doit exister dès la V1.

## 19. Logs et diagnostic

Le module doit permettre d’observer au minimum :

- démarrage du serveur ;
- arrêt du serveur ;
- port écouté ;
- nombre maximal de clients autorisés ;
- acceptation d’un client ;
- rejet d’un client faute de slot ;
- fermeture normale ;
- fermeture sur erreur ;
- erreurs socket ;
- saturation du buffer TX ;
- coût mémoire mesuré après compilation.

Les logs doivent rester compatibles avec un environnement ESP8266 contraint.

## 20. Erreurs attendues

Le module doit traiter explicitement :

- échec de création du socket serveur ;
- échec de bind ;
- échec de listen ;
- échec de passage en non-bloquant ;
- erreur `select()` ;
- erreur `accept()` ;
- erreur `recv()` ;
- erreur `send()` ;
- client fermé côté distant ;
- buffer TX insuffisant ;
- demande d’envoi sur connexion invalide ;
- demande de fermeture sur slot déjà libre.

## 21. Contraintes de conception

Le module doit respecter les contraintes suivantes :

- C pur ;
- pas de vtable générique ;
- pas de polymorphisme dynamique ;
- pas d’allocation dynamique par client ;
- pas de queue applicative ;
- pas de dépendance HTTP ;
- pas d’exposition directe de lwIP aux couches supérieures ;
- buffers bornés ;
- états explicites ;
- comportement déterministe autant que possible.

## 22. Dépendance du futur HTTP server

Le futur `http_server` devra dépendre uniquement du contrat transport.

Il ne devra pas :

- manipuler directement les sockets lwIP ;
- connaître les détails de `select()` ;
- gérer lui-même les slots clients TCP ;
- contourner les règles de fermeture ;
- imposer une architecture réseau différente.

Le rôle du transport est de fournir un flux d’octets borné et stable.

Le rôle du HTTP minimal sera d’interpréter ces octets.

## 23. Critères d’acceptation fonctionnelle

Le module est considéré conforme si :

1. le serveur démarre sur un port donné ;
2. le serveur accepte jusqu’à `max_clients` clients ;
3. `max_clients` ne peut pas dépasser `TCP_SERVER_MAX_CLIENTS` ;
4. un client supplémentaire est accepté puis fermé immédiatement ;
5. chaque client actif occupe un slot statique ;
6. la réception déclenche `on_data` ;
7. l’émission via `tcp_send()` ne bloque pas ;
8. les envois partiels reprennent correctement ;
9. `tcp_close_after_drain()` ferme immédiatement si le TX est vide ;
10. `tcp_close_after_drain()` ferme après émission si le TX est non vide ;
11. `tcp_close()` libère correctement le slot ;
12. `tcp_server_stop()` ferme tous les fd actifs ;
13. aucun callback ne s’exécute hors task réseau ;
14. aucune queue applicative n’est créée ;
15. aucun traitement HTTP n’est présent dans le module ;
16. le coût RAM est mesuré et documenté après compilation ;
17. les erreurs socket sont loguées et conduisent à une fermeture contrôlée.

## 24. Scénarios de test fonctionnel

### Scénario 1 — Démarrage simple

- Démarrer le serveur sur un port valide.
- Vérifier que le socket écoute.
- Vérifier que la task réseau est active.

Résultat attendu : serveur prêt, aucun client connecté.

### Scénario 2 — Connexion client unique

- Connecter un client TCP.
- Vérifier l’attribution d’un slot.
- Vérifier l’appel à `on_connect`.

Résultat attendu : connexion active en état `TCP_SLOT_USED`.

### Scénario 3 — Réception de données

- Envoyer des octets depuis le client.
- Vérifier le remplissage RX.
- Vérifier l’appel à `on_data`.

Résultat attendu : données transmises à l’application sans blocage.

### Scénario 4 — Émission partielle

- Demander un envoi via `tcp_send()`.
- Forcer ou simuler une écriture partielle.
- Vérifier la reprise via `tx_offset`.

Résultat attendu : toutes les données acceptées sont envoyées progressivement.

### Scénario 5 — Saturation clients

- Connecter `TCP_SERVER_MAX_CLIENTS` clients.
- Connecter un client supplémentaire.

Résultat attendu : le client supplémentaire est accepté puis fermé immédiatement.

### Scénario 6 — Fermeture client

- Fermer la connexion côté client.
- Vérifier la détection côté serveur.
- Vérifier l’appel à `on_close`.
- Vérifier la libération du slot.

Résultat attendu : slot réutilisable.

### Scénario 7 — Arrêt serveur

- Démarrer le serveur.
- Connecter plusieurs clients.
- Appeler `tcp_server_stop()`.

Résultat attendu : tous les fd sont fermés, la task est arrêtée, les slots sont réinitialisés.

### Scénario 8 — Fermeture après vidage TX vide

- Connecter un client TCP.
- Appeler `tcp_close_after_drain()` sans `tcp_send()`.
- Vérifier l’appel unique à `on_close`.
- Vérifier la libération du slot.

Résultat attendu : fermeture immédiate et slot réutilisable.

### Scénario 9 — Fermeture après vidage TX non vide

- Depuis un callback, appeler `tcp_send(conn, "OK", 2)`.
- Appeler ensuite `tcp_close_after_drain(conn)`.
- Lire côté client jusqu’à EOF.

Résultat attendu : le client reçoit exactement `OK`, puis la connexion est fermée côté serveur.

### Scénario 10 — Envoi refusé après demande de drain-close

- Appeler `tcp_send(conn, data1, len1)`.
- Appeler `tcp_close_after_drain(conn)`.
- Appeler `tcp_send(conn, data2, len2)`.

Résultat attendu : le second `tcp_send()` retourne `0` et seules les données déjà acceptées avant drain-close sont envoyées.

## 25. Mesures obligatoires après implémentation

Après compilation et test sur cible, documenter :

- taille firmware ajoutée par le module ;
- RAM statique consommée ;
- stack réellement consommée par la task réseau ;
- RAM libre au démarrage ;
- RAM libre avec 1 client ;
- RAM libre avec 3 clients ;
- comportement lors d’une saturation TX ;
- comportement lors d’une déconnexion brutale ;
- impact avant ajout de HTTP.

## 26. Points ouverts

Les points suivants restent à arbitrer après première implémentation :

- taille définitive des buffers RX/TX : 256 ou 512 octets ;
- taille de stack exacte de la task réseau ;
- timeout d’inactivité par défaut ;
- granularité des logs ;
- format standard des erreurs ;
- nommage final des fichiers publics et internes ;
- stratégie future pour appels cross-task ;
- compatibilité éventuelle avec d’autres projets ESP.

## 27. Synthèse fonctionnelle

La brique attendue est un serveur TCP minimal, non bloquant, fondé sur une task réseau unique et des slots statiques.

Elle doit résoudre le problème de fond : fournir un socle réseau propre avant HTTP, sans exploser la RAM, sans exposer lwIP aux couches supérieures et sans enfermer les projets futurs dans une architecture réseau fragile.

La V1 privilégie la stabilité, la lisibilité, la mesure mémoire et la maîtrise du comportement plutôt que la généralisation prématurée.
