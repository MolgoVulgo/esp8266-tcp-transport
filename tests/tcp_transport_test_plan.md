# Test Plan / Plan de test - ESP8266 TCP Transport

## English

### Preconditions

- Firmware built with `include/esp8266_tcp_transport.h` and `src/esp8266_tcp_transport.c`.
- Wi-Fi initialized before `tcp_server_start()`.
- Test application providing short callbacks:
  - `on_connect`: fd log;
  - `on_data`: optional echo through `tcp_send()`;
  - `on_close`: close log with reason;
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

16. TX drain callback for chunked response
   - In `on_data`, queue first chunk with `tcp_send()`.
   - Implement `on_drain` to queue next chunk while data remains, then call `tcp_close_after_drain()` for the final chunk.
   - Expected: each chunk is sent in order, `on_drain` is called only after a non-empty TX buffer is fully drained, no `on_drain` after final drain-close.

17. TX helper functions
   - During callbacks, check `tcp_tx_available(conn)` and `tcp_tx_empty(conn)` before and after `tcp_send()`, during partial-drain, and after full drain.
   - Expected: available decreases when bytes are accepted, returns to full size once drained, and `tcp_tx_empty()` reflects pending bytes state.

### Points to Observe

- Logs for start, stop, accept, reject, socket errors and TX saturation.
- No long or blocking callback.
- No HTTP processing.
- Free heap before start, after start, with 1 client and with 3 clients.
- Network task stack high watermark if exposed by the project API.

### Named Contract Checks

- `test_tcp_tx_available_empty`: with empty TX (`tx_len = 0`, `tx_offset = 0`), expect `tcp_tx_available() == TCP_TX_BUFFER_SIZE`.
- `test_tcp_tx_available_full`: with full pending TX, expect `tcp_tx_available() == 0`.
- `test_tcp_tx_available_with_offset`: with `tx_len = 512`, `tx_offset = 256`, expect pending `256` and available `TCP_TX_BUFFER_SIZE - 256`.
- `test_tcp_tx_empty_empty`: with empty TX, expect `tcp_tx_empty() == true`.
- `test_tcp_tx_empty_pending`: with pending bytes (`tx_offset < tx_len`), expect `tcp_tx_empty() == false`.
- `test_tcp_tx_empty_with_offset_equal_len`: with `tx_offset == tx_len`, expect `tcp_tx_empty() == true`.
- `test_on_drain_called_after_tx_empty`: expect `on_drain` when TX transitions from pending to empty and connection stays open.
- `test_on_drain_not_called_after_close_after_drain`: with `close_after_drain == true`, expect close on final drain and no `on_drain`.
- `test_tcp_send_allowed_from_on_drain`: call `tcp_send()` from `on_drain`, expect accepted bytes when space is available.
- `test_multiple_send_drain_cycles`: run several send->drain->send cycles, expect ordered delivery and stable callbacks.
- `test_on_close_once_after_close_after_drain`: expect one `on_close` callback only.
- `test_no_on_drain_after_close`: once closed, no later `on_drain` callback.
- `test_on_drain_null_keeps_previous_behavior`: with `on_drain == NULL`, expect no callback side effect and normal TX close behavior.

## Francais

### Preconditions

- Firmware compile avec `include/esp8266_tcp_transport.h` et `src/esp8266_tcp_transport.c`.
- Wi-Fi initialise avant `tcp_server_start()`.
- Application de test fournissant des callbacks courts :
  - `on_connect` : log du fd ;
  - `on_data` : echo optionnel via `tcp_send()` ;
  - `on_close` : log de fermeture avec raison ;
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

16. Callback de drainage TX pour reponse en morceaux
   - Dans `on_data`, pousser un premier bloc via `tcp_send()`.
   - Implementer `on_drain` pour pousser le bloc suivant tant qu'il reste des donnees, puis appeler `tcp_close_after_drain()` au dernier bloc.
   - Attendu : blocs emis dans l'ordre, `on_drain` appele uniquement apres vidage d'un TX non vide, aucun `on_drain` apres fermeture finale par drain-close.

17. Fonctions utilitaires TX
   - Pendant les callbacks, verifier `tcp_tx_available(conn)` et `tcp_tx_empty(conn)` avant/apres `tcp_send()`, pendant un drain partiel, puis apres drain complet.
   - Attendu : l'espace disponible diminue quand des octets sont acceptes, revient au maximum apres vidage, et `tcp_tx_empty()` reflete correctement l'etat d'attente.

### Points a observer

- Logs de demarrage, arret, acceptation, rejet, erreurs socket et saturation TX.
- Absence de callback long ou bloquant.
- Absence de traitement HTTP.
- Heap libre avant demarrage, apres demarrage, avec 1 client et avec 3 clients.
- Stack high watermark de la task reseau si l'API projet l'expose.

### Verifications de contrat nommees

- `test_tcp_tx_available_empty` : TX vide (`tx_len = 0`, `tx_offset = 0`), attendu `tcp_tx_available() == TCP_TX_BUFFER_SIZE`.
- `test_tcp_tx_available_full` : TX en attente complet, attendu `tcp_tx_available() == 0`.
- `test_tcp_tx_available_with_offset` : `tx_len = 512`, `tx_offset = 256`, attendu pending `256` et disponible `TCP_TX_BUFFER_SIZE - 256`.
- `test_tcp_tx_empty_empty` : TX vide, attendu `tcp_tx_empty() == true`.
- `test_tcp_tx_empty_pending` : octets en attente (`tx_offset < tx_len`), attendu `tcp_tx_empty() == false`.
- `test_tcp_tx_empty_with_offset_equal_len` : `tx_offset == tx_len`, attendu `tcp_tx_empty() == true`.
- `test_on_drain_called_after_tx_empty` : `on_drain` appele quand le TX passe de non vide a vide avec connexion ouverte.
- `test_on_drain_not_called_after_close_after_drain` : avec `close_after_drain == true`, fermeture au vidage final et aucun `on_drain`.
- `test_tcp_send_allowed_from_on_drain` : appel `tcp_send()` depuis `on_drain`, octets acceptes si place disponible.
- `test_multiple_send_drain_cycles` : plusieurs cycles send->drain->send, emission ordonnee et callbacks stables.
- `test_on_close_once_after_close_after_drain` : un seul appel `on_close`.
- `test_no_on_drain_after_close` : apres fermeture, aucun `on_drain` ulterieur.
- `test_on_drain_null_keeps_previous_behavior` : avec `on_drain == NULL`, pas d'effet de bord callback et comportement TX/fermeture normal.
