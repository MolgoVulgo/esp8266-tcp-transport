# TCP Echo Server Example / Exemple serveur echo TCP

## English

Native ESP8266 RTOS SDK example for ESP8266 RTOS SDK.

The firmware connects the ESP8266 as a Wi-Fi station, starts `esp8266-tcp-transport` on `TCP_ECHO_PORT`, then sends each received TCP block back to the client.

### Configuration

Create the local Wi-Fi credentials file from the template:

```text
cp main/wifi_credentials.example.h main/wifi_credentials.h
```

Then edit `main/wifi_credentials.h`.

Override the port at build time if needed:

```sh
idf.py -D TCP_ECHO_PORT=7777 build
```

### Build

From this example directory:

```sh
idf.py build
```

### Manual Test

After flashing, once the target is reachable on the network, run:

```sh
python3 examples/tcp_echo_server/tools/tcp_echo_client.py <esp8266-ip> 7777 "ping"
```

Expected result:

```text
sent: 4 bytes
received: ping
```

Expected firmware logs include:

```text
wifi connected ip=...
client accepted fd=... remote_port=...
client closed reason=1 last_error=0
```

## Francais

Exemple natif ESP8266 RTOS SDK pour ESP8266 RTOS SDK.

Le firmware connecte l'ESP8266 en Wi-Fi station, demarre `esp8266-tcp-transport` sur `TCP_ECHO_PORT`, puis renvoie chaque bloc TCP recu au client.

### Configuration

Creer le fichier local d'identifiants Wi-Fi a partir du modele :

```text
cp main/wifi_credentials.example.h main/wifi_credentials.h
```

Puis editer `main/wifi_credentials.h`.

Surcharger le port au build si necessaire :

```sh
idf.py -D TCP_ECHO_PORT=7777 build
```

### Build

Depuis le dossier de l'exemple :

```sh
idf.py build
```

### Test manuel

Apres flash, une fois la cible joignable sur le reseau, lancer :

```sh
python3 examples/tcp_echo_server/tools/tcp_echo_client.py <ip-esp8266> 7777 "ping"
```

Resultat attendu :

```text
sent: 4 bytes
received: ping
```

Logs firmware attendus :

```text
wifi connected ip=...
client accepted fd=... remote_port=...
client closed reason=1 last_error=0
```
