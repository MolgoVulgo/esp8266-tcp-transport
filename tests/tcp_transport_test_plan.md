# Test Plan / Plan de test - ESP8266 TCP Transport

## English

### Preconditions

- Firmware built with `include/tcp_transport.h` and `src/tcp_transport.c`.
- Wi-Fi initialized before `tcp_server_start()`.
- Test application providing short callbacks:
  - `on_connect`: fd log;
  - `on_data`: optional echo through `tcp_send()`;
  - `on_close`: close log;
  - `on_error`: error log.

### Minimal Scenarios

1. Server start
   - Call `tcp_server_start(port, 3, &callbacks)`.
   - Expected: `TCP_TRANSPORT_OK`, startup log, no active client.

2. Invalid parameters
   - Test port `0`, `max_clients = 0`, `max_clients > TCP_SERVER_MAX_CLIENTS`.
   - Expected: `TCP_TRANSPORT_ERR_INVALID_ARG`, no server socket left open.

3. Single connection
   - Connect one client with `nc <ip> <port>`.
   - Expected: slot in `TCP_SLOT_USED`, `on_connect` callback.

4. Receive
   - Send a short line from the client.
   - Expected: `on_data` callback, correct received length, no blocking.

5. Transmit
   - From `on_data`, call `tcp_send(conn, buf, len)` with a short message.
   - Expected: client receives the bytes, return value equals length when the buffer is free.

6. Client saturation
   - Connect three clients.
   - Connect a fourth client.
   - Expected: fourth connection accepted then closed, rejection log, three active slots unchanged.

7. TX saturation
   - Call `tcp_send()` with more than `TCP_TX_BUFFER_SIZE` bytes.
   - Expected: partial return or zero, saturation log, no buffer overflow.

8. Remote close
   - Close the client.
   - Expected: `on_close` callback, fd closed, slot reusable.

9. Server stop
   - With several active clients, call `tcp_server_stop()`.
   - Expected: all clients closed, task stopped, server restartable.

10. Restart
    - Call `tcp_server_start()` after a complete stop.
    - Expected: server operational on the same port.

11. Close after empty TX drain
    - Connect one client.
    - From `on_connect`, call `tcp_close_after_drain(conn)` without `tcp_send()`.
    - Expected: clean close, slot freed, `on_close` called once.

12. Close after non-empty TX drain
    - From `on_data`, call `tcp_send(conn, "OK", 2)` then `tcp_close_after_drain(conn)`.
    - Read client-side until EOF.
    - Expected: client receives exactly `OK`, then the server closes the connection.

13. Partial send then close after drain
    - Force or simulate a partial `send()` on a response already accepted in `tx_buf`.
    - Call `tcp_close_after_drain(conn)`.
    - Expected: `tx_offset` progresses over several passes, close only when `tx_offset == tx_len`, no accepted byte is lost.

14. Send refused after drain-close
    - Call `tcp_send(conn, data1, len1)`.
    - Call `tcp_close_after_drain(conn)`.
    - Call `tcp_send(conn, data2, len2)`.
    - Expected: second `tcp_send()` returns `0`, only previously accepted `data1` is sent.

15. Error during drain-close
    - Enable `close_after_drain` with pending TX data.
    - Abruptly cut the client before it reads all data.
    - Expected: socket error logged, `on_error` called, fd closed, slot freed, network loop not blocked.

### Points to Observe

- Logs for start, stop, accept, reject, socket errors and TX saturation.
- No long or blocking callback.
- No HTTP processing.
- Free heap before start, after start, with 1 client and with 3 clients.
- Network task stack high watermark if exposed by the project API.

## Francais

### Preconditions

- Firmware compile avec `include/tcp_transport.h` et `src/tcp_transport.c`.
- Wi-Fi initialise avant `tcp_server_start()`.
- Application de test fournissant des callbacks courts :
  - `on_connect` : log du fd ;
  - `on_data` : echo optionnel via `tcp_send()` ;
  - `on_close` : log de fermeture ;
  - `on_error` : log d'erreur.

### Scenarios minimaux

1. Demarrage serveur
   - Appeler `tcp_server_start(port, 3, &callbacks)`.
   - Attendu : `TCP_TRANSPORT_OK`, log de demarrage, aucun client actif.

2. Parametres invalides
   - Tester port `0`, `max_clients = 0`, `max_clients > TCP_SERVER_MAX_CLIENTS`.
   - Attendu : `TCP_TRANSPORT_ERR_INVALID_ARG`, aucun socket serveur ouvert.

3. Connexion unique
   - Connecter un client avec `nc <ip> <port>`.
   - Attendu : slot en `TCP_SLOT_USED`, callback `on_connect`.

4. Reception
   - Envoyer une ligne courte depuis le client.
   - Attendu : callback `on_data`, longueur recue correcte, pas de blocage.

5. Emission
   - Depuis `on_data`, appeler `tcp_send(conn, buf, len)` avec un message court.
   - Attendu : le client recoit les octets, retour egal a la longueur si le buffer est libre.

6. Saturation clients
   - Connecter trois clients.
   - Connecter un quatrieme client.
   - Attendu : quatrieme connexion acceptee puis fermee, log de rejet, trois slots actifs inchanges.

7. Saturation TX
   - Appeler `tcp_send()` avec plus de `TCP_TX_BUFFER_SIZE` octets.
   - Attendu : retour partiel ou zero, log de saturation, aucun depassement de buffer.

8. Fermeture distante
   - Fermer le client.
   - Attendu : callback `on_close`, fd ferme, slot reutilisable.

9. Arret serveur
   - Avec plusieurs clients actifs, appeler `tcp_server_stop()`.
   - Attendu : fermeture de tous les clients, task arretee, serveur redemarrable.

10. Redemarrage
    - Appeler `tcp_server_start()` apres un arret complet.
    - Attendu : serveur operationnel sur le meme port.

11. Fermeture apres vidage TX vide
    - Connecter un client.
    - Depuis `on_connect`, appeler `tcp_close_after_drain(conn)` sans `tcp_send()`.
    - Attendu : connexion fermee proprement, slot libere, `on_close` appele une seule fois.

12. Fermeture apres vidage TX non vide
    - Depuis `on_data`, appeler `tcp_send(conn, "OK", 2)` puis `tcp_close_after_drain(conn)`.
    - Lire cote client jusqu'a EOF.
    - Attendu : le client recoit exactement `OK`, puis la connexion est fermee par le serveur.

13. Envoi partiel puis fermeture apres drain
    - Forcer ou simuler un `send()` partiel sur une reponse deja acceptee dans `tx_buf`.
    - Appeler `tcp_close_after_drain(conn)`.
    - Attendu : `tx_offset` progresse sur plusieurs passages, fermeture uniquement quand `tx_offset == tx_len`, aucun octet accepte n'est perdu.

14. Envoi refuse apres drain-close
    - Appeler `tcp_send(conn, data1, len1)`.
    - Appeler `tcp_close_after_drain(conn)`.
    - Appeler `tcp_send(conn, data2, len2)`.
    - Attendu : le second `tcp_send()` retourne `0`, seules les donnees de `data1` deja acceptees sont envoyees.

15. Erreur pendant drain-close
    - Activer `close_after_drain` avec des donnees TX en attente.
    - Couper brutalement le client avant lecture complete.
    - Attendu : erreur socket loguee, `on_error` appele, fd ferme, slot libere, boucle reseau non bloquee.

### Points a observer

- Logs de demarrage, arret, acceptation, rejet, erreurs socket et saturation TX.
- Absence de callback long ou bloquant.
- Absence de traitement HTTP.
- Heap libre avant demarrage, apres demarrage, avec 1 client et avec 3 clients.
- Stack high watermark de la task reseau si l'API projet l'expose.
