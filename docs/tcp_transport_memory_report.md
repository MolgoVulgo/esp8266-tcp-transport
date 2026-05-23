# Rapport mémoire — Transport TCP ESP8266

## Configuration mesurée

À remplir après compilation sur le projet cible.

| Élément | Valeur |
|---|---:|
| `TCP_SERVER_MAX_CLIENTS` | 3 |
| `TCP_RX_BUFFER_SIZE` | 512 octets |
| `TCP_TX_BUFFER_SIZE` | 512 octets |
| `TCP_NETWORK_TASK_STACK_SIZE` | 1024 |
| `TCP_SELECT_TIMEOUT_MS` | 100 ms |
| `TCP_IDLE_TIMEOUT_MS` | 5000 ms |

## Mesures firmware

| Mesure | Valeur |
|---|---:|
| Taille firmware avant module | À mesurer |
| Taille firmware après module | À mesurer |
| Différence firmware | À mesurer |
| RAM statique ajoutée | À mesurer |

## Mesures runtime

| Scénario | Heap libre | Stack task réseau | Observation |
|---|---:|---:|---|
| Serveur arrêté | À mesurer | N/A |  |
| Serveur démarré, 0 client | À mesurer | À mesurer |  |
| Serveur démarré, 1 client | À mesurer | À mesurer |  |
| Serveur démarré, 3 clients | À mesurer | À mesurer |  |
| Saturation TX | À mesurer | À mesurer |  |
| Rejet 4e client | À mesurer | À mesurer |  |
| Déconnexion brutale | À mesurer | À mesurer |  |

## Décisions après mesure

- Taille RX/TX finale : à décider.
- Taille de stack finale : à décider.
- Timeout d’inactivité par défaut : 5000 ms, désactivable avec `TCP_IDLE_TIMEOUT_MS=0`.
- Niveau de logs permanent : à décider.
