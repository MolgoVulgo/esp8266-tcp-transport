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

11. Fermeture après vidage TX vide
    - Connecter un client.
    - Depuis `on_connect`, appeler `tcp_close_after_drain(conn)` sans `tcp_send()`.
    - Attendu : connexion fermée proprement, slot libéré, `on_close` appelé une seule fois.

12. Fermeture après vidage TX non vide
    - Depuis `on_data`, appeler `tcp_send(conn, "OK", 2)` puis `tcp_close_after_drain(conn)`.
    - Lire côté client jusqu’à EOF.
    - Attendu : le client reçoit exactement `OK`, puis la connexion est fermée par le serveur.

13. Envoi partiel puis fermeture après drain
    - Forcer ou simuler un `send()` partiel sur une réponse déjà acceptée dans `tx_buf`.
    - Appeler `tcp_close_after_drain(conn)`.
    - Attendu : `tx_offset` progresse sur plusieurs passages, fermeture uniquement quand `tx_offset == tx_len`, aucun octet accepté n’est perdu.

14. Envoi refusé après drain-close
    - Appeler `tcp_send(conn, data1, len1)`.
    - Appeler `tcp_close_after_drain(conn)`.
    - Appeler `tcp_send(conn, data2, len2)`.
    - Attendu : le second `tcp_send()` retourne `0`, seules les données de `data1` déjà acceptées sont envoyées.

15. Erreur pendant drain-close
    - Activer `close_after_drain` avec des données TX en attente.
    - Couper brutalement le client avant lecture complète.
    - Attendu : erreur socket loguée, `on_error` appelé, fd fermé, slot libéré, boucle réseau non bloquée.

## Points à observer

- Logs de démarrage, arrêt, acceptation, rejet, erreurs socket et saturation TX.
- Absence de callback long ou bloquant.
- Absence de traitement HTTP.
- Heap libre avant démarrage, après démarrage, avec 1 client et avec 3 clients.
- Stack high watermark de la task réseau si l’API projet l’expose.
