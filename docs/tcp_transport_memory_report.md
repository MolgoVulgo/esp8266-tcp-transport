# Memory Report / Rapport memoire - ESP8266 TCP Transport

## English

### Measured Configuration

Fill this section after building the target firmware.

| Item | Value |
|---|---:|
| `TCP_SERVER_MAX_CLIENTS` | 3 |
| `TCP_RX_BUFFER_SIZE` | 512 bytes |
| `TCP_TX_BUFFER_SIZE` | 512 bytes |
| `TCP_NETWORK_TASK_STACK_SIZE` | 1024 |
| `TCP_SELECT_TIMEOUT_MS` | 100 ms |
| `TCP_IDLE_TIMEOUT_MS` | 5000 ms |

### Firmware Measurements

| Measurement | Value |
|---|---:|
| Firmware size before module | To measure |
| Firmware size after module | To measure |
| Firmware size difference | To measure |
| Added static RAM | To measure |

### Runtime Measurements

| Scenario | Free heap | Network task stack | Observation |
|---|---:|---:|---|
| Server stopped | To measure | N/A |  |
| Server started, 0 client | To measure | To measure |  |
| Server started, 1 client | To measure | To measure |  |
| Server started, 3 clients | To measure | To measure |  |
| TX saturation | To measure | To measure |  |
| 4th client rejection | To measure | To measure |  |
| Abrupt client disconnect | To measure | To measure |  |

### Decisions After Measurement

- Final RX/TX buffer size: to decide.
- Final network task stack size: to decide.
- Default idle timeout: 5000 ms, disabled with `TCP_IDLE_TIMEOUT_MS=0`.
- Permanent log level: to decide.

## Francais

### Configuration mesuree

Remplir cette section apres compilation du firmware cible.

| Element | Valeur |
|---|---:|
| `TCP_SERVER_MAX_CLIENTS` | 3 |
| `TCP_RX_BUFFER_SIZE` | 512 octets |
| `TCP_TX_BUFFER_SIZE` | 512 octets |
| `TCP_NETWORK_TASK_STACK_SIZE` | 1024 |
| `TCP_SELECT_TIMEOUT_MS` | 100 ms |
| `TCP_IDLE_TIMEOUT_MS` | 5000 ms |

### Mesures firmware

| Mesure | Valeur |
|---|---:|
| Taille firmware avant module | A mesurer |
| Taille firmware apres module | A mesurer |
| Difference de taille firmware | A mesurer |
| RAM statique ajoutee | A mesurer |

### Mesures runtime

| Scenario | Heap libre | Stack task reseau | Observation |
|---|---:|---:|---|
| Serveur arrete | A mesurer | N/A |  |
| Serveur demarre, 0 client | A mesurer | A mesurer |  |
| Serveur demarre, 1 client | A mesurer | A mesurer |  |
| Serveur demarre, 3 clients | A mesurer | A mesurer |  |
| Saturation TX | A mesurer | A mesurer |  |
| Rejet du 4e client | A mesurer | A mesurer |  |
| Deconnexion brutale client | A mesurer | A mesurer |  |

### Decisions apres mesure

- Taille RX/TX finale : a decider.
- Taille de stack finale de la task reseau : a decider.
- Timeout d'inactivite par defaut : 5000 ms, desactivable avec `TCP_IDLE_TIMEOUT_MS=0`.
- Niveau de logs permanent : a decider.
