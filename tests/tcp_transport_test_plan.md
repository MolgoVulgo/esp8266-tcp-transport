# Plan de test — Transport TCP ESP8266

## Préconditions

- Firmware compilé avec `include/tcp_transport.h` et `src/tcp_transport.c`.
- Wi-Fi initialisé avant `tcp_server_start()`.
- Application de test fournissant des callbacks courts :
  - `on_connect` : log du fd ;
  - `on_data` : optionnellement echo via `tcp_send()` ;
  - `on_close` : log fermeture ;
  - `on_error` : log erreur.

## Scénarios minimaux

1. Démarrage serveur
   - Appeler `tcp_server_start(port, 3, &callbacks)`.
   - Attendu : retour `TCP_TRANSPORT_OK`, log de démarrage, aucun client actif.

2. Paramètres invalides
   - Tester port `0`, `max_clients = 0`, `max_clients > TCP_SERVER_MAX_CLIENTS`.
   - Attendu : retour `TCP_TRANSPORT_ERR_INVALID_ARG`, aucun socket serveur ouvert.

3. Connexion unique
   - Connecter un client avec `nc <ip> <port>`.
   - Attendu : slot en `TCP_SLOT_USED`, callback `on_connect`.

4. Réception
   - Envoyer une ligne courte depuis le client.
   - Attendu : callback `on_data`, longueur reçue correcte, pas de blocage.

5. Émission
   - Depuis `on_data`, appeler `tcp_send(conn, buf, len)` avec un message court.
   - Attendu : le client reçoit les octets, retour égal à la longueur si buffer libre.

6. Saturation clients
   - Connecter trois clients.
   - Connecter un quatrième client.
   - Attendu : quatrième connexion acceptée puis fermée, log de rejet, trois slots actifs inchangés.

7. Saturation TX
   - Appeler `tcp_send()` avec plus de `TCP_TX_BUFFER_SIZE` octets.
   - Attendu : retour partiel ou zéro, log de saturation, aucun dépassement de buffer.

8. Fermeture distante
   - Fermer le client.
   - Attendu : callback `on_close`, fd fermé, slot réutilisable.

9. Fermeture serveur
   - Avec plusieurs clients actifs, appeler `tcp_server_stop()`.
   - Attendu : fermeture de tous les clients, task arrêtée, serveur redémarrable.

10. Redémarrage
    - Appeler `tcp_server_start()` après un arrêt complet.
    - Attendu : serveur opérationnel sur le même port.

## Points à observer

- Logs de démarrage, arrêt, acceptation, rejet, erreurs socket et saturation TX.
- Absence de callback long ou bloquant.
- Absence de traitement HTTP.
- Heap libre avant démarrage, après démarrage, avec 1 client et avec 3 clients.
- Stack high watermark de la task réseau si l’API projet l’expose.
