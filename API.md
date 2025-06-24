# 📚 Documentation API - Arduino mDNS MQTT Client

## Table des Matières

- [Vue d'ensemble](#vue-densemble)
- [Fonctions Principales](#fonctions-principales)
- [Configuration](#configuration)
- [Gestion des États](#gestion-des-états)
- [Messages et Protocoles](#messages-et-protocoles)
- [Gestion d'Erreurs](#gestion-derreurs)
- [Optimisations](#optimisations)

---

## Vue d'ensemble

Le client Arduino mDNS MQTT est organisé autour de plusieurs modules fonctionnels
 qui interagissent pour fournir une connectivité MQTT robuste avec découverte automatique.

### Architecture Générale

```text
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   WiFi Module   │────│   mDNS Module   │────│  MQTT Module    │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         └───────────────────────┼───────────────────────┘
                                 │
                    ┌─────────────────┐
                    │   RTC Module    │
                    └─────────────────┘
```

---

## Fonctions Principales

### 🌐 Gestion WiFi

#### `void connectToWiFi()`

**Description**: Établit la connexion WiFi en utilisant les credentials configurés.

**Comportement**:

- Utilise `SECRET_SSID` et `SECRET_PASS` depuis `arduino_secrets.h`
- Boucle d'attente non-bloquante avec indicateur de progression
- Affiche l'adresse IP obtenue une fois connecté

**Messages de debug**:

```text
Connexion au réseau WiFi: MonWiFi
....
WiFi connecté!
Adresse IP: 192.168.1.100
```

**Gestion d'erreurs**:

- Retry automatique en cas d'échec
- Pas de timeout (connexion persistante)

---

### 🔍 Découverte mDNS

#### `void searchForMQTTServer()`

**Description**: Envoie une requête mDNS pour découvrir les serveurs MQTT sur le réseau local.

**Protocole**:

- Service recherché: `_mqtt._tcp.local` (configurable)
- Port multicast: `224.0.0.251:5353`
- Format: Requête PTR standard mDNS

**Paramètres configurables**:

```cpp
MDNS_SERVICE_TYPE    // "mqtt", "mqtts", "mosquitto"
MDNS_PROTOCOL        // "tcp", "udp"  
MDNS_DOMAIN          // "local"
```

**Algorithme**:

1. Construction du nom de service dynamique via `buildMDNSServiceName()`
2. Création du paquet mDNS avec header standard
3. Encodage du nom de service avec longueurs préfixées
4. Envoi UDP multicast

#### `String buildMDNSServiceName()`

**Description**: Construit le nom complet du service mDNS à rechercher.

**Retour**: String au format `_service._protocol.domain.`

**Exemple**: `_mqtt._tcp.local.`

#### `void buildMDNSQuery(byte* query, int* queryLength, String serviceName)`

**Description**: Encode un nom de service dans le format mDNS avec longueurs préfixées.

**Paramètres**:

- `query`: Buffer de sortie pour la requête encodée
- `queryLength`: Pointeur vers la longueur courante (modifié)
- `serviceName`: Nom du service à encoder

**Format de sortie**:

```text
[4]mqtt[3]_tcp[5]local[0]
 │    │   │     │     │
 └─longueur des labels─┘
```

#### `void listenForMDNSResponses()`

**Description**: Écoute et analyse les réponses mDNS pour identifier les serveurs MQTT.

**Algorithme**:

1. Vérification de paquets UDP entrants
2. Lecture dans buffer fixe (`packetBuffer[512]`)
3. Validation via `isMQTTResponse()`
4. Sauvegarde de l'IP du serveur si détecté

#### `bool isMQTTResponse(byte *data, int length)`

**Description**: Analyse un paquet mDNS pour détecter les services MQTT.

**Paramètres**:

- `data`: Buffer contenant le paquet reçu
- `length`: Taille du paquet en bytes

**Retour**: `true` si le paquet contient un service MQTT

**Logique de détection**:

1. Vérification du flag "Response" dans l'header mDNS
2. Recherche de la chaîne de service configurée
3. Support des formats encodés et non-encodés

---

### 📡 Gestion MQTT

#### `void connectToMQTT()`

**Description**: Établit la connexion au serveur MQTT découvert.

**Processus**:

1. Configuration du client avec IP et port du serveur
2. Test de connectivité TCP préliminaire
3. Connexion MQTT avec client ID unique
4. Gestion des codes d'erreur MQTT

**Client ID**: Format `Arduino-{dernier_octet_IP}`

**Codes d'erreur gérés**:

```cpp
-4: MQTT_CONNECTION_TIMEOUT
-3: MQTT_CONNECTION_LOST  
-2: MQTT_CONNECT_FAILED (TCP échec)
-1: MQTT_DISCONNECTED
 1: MQTT_CONNECT_BAD_PROTOCOL
 2: MQTT_CONNECT_BAD_CLIENT_ID
 3: MQTT_CONNECT_UNAVAILABLE
 4: MQTT_CONNECT_BAD_CREDENTIALS
 5: MQTT_CONNECT_UNAUTHORIZED
```

#### `void publishHeartbeat()`

**Description**: Publie un message de heartbeat sur le topic MQTT configuré.

**Format du message**:

```text
Template: HEARTBEAT_MESSAGE_FORMAT
Défaut: "%s vous dit bonjour. Il est %s"
Exemple: "192.168.1.100 vous dit bonjour. Il est 14:35:22"
```

**Données incluses**:

- Adresse IP locale (formatée avec `snprintf`)
- Timestamp RTC ou fallback si non synchronisé

**Topic**: Défini par `MQTT_TOPIC` (défaut: `/arduino`)

---

### ⏰ Gestion RTC

#### `void initializeRTC()`

**Description**: Initialise le module RTC du MKR 1010.

**Opérations**:

- Démarrage du module RTCZero
- Préparation pour synchronisation réseau

#### `void tryToSyncRTC()`

**Description**: Tente de synchroniser l'horloge RTC avec le temps réseau.

**Méthode**:

- Utilise `WiFi.getTime()` pour obtenir l'epoch UNIX
- Applique directement avec `rtc.setEpoch(epochTime)`
- Retry automatique toutes les `RTC_SYNC_INTERVAL` ms

**États**:

- `rtcInitialized = false`: Synchronisation en cours
- `rtcInitialized = true`: RTC synchronisé et fonctionnel

#### `void printCurrentTime()`

**Description**: Affiche l'heure actuelle formatée.

**Format**: `DD/MM/YYYY HH:MM:SS`

**Exemple**: `23/06/2025 14:35:22`

---

## Configuration

### Variables d'Environnement

#### Fichier `arduino_secrets.h`

```cpp
#define SECRET_SSID "NomWiFi"      // SSID du réseau WiFi
#define SECRET_PASS "MotDePasse"   // Mot de passe WiFi
```

#### Fichier `config.h`

```cpp
// Debug
#define DEBUG true/false           // Active/désactive messages série

// MQTT  
#define MQTT_TOPIC "/arduino"      // Topic de publication
#define MQTT_PORT 1883             // Port serveur MQTT
#define MQTT_CLIENT_PREFIX "Arduino" // Préfixe client ID

// mDNS
#define MDNS_SERVICE_TYPE "mqtt"   // Type de service recherché
#define MDNS_PROTOCOL "tcp"        // Protocole (tcp/udp)
#define MDNS_DOMAIN "local"        // Domaine mDNS

// Timing
#define SEARCH_INTERVAL 30000      // Intervalle recherche mDNS (ms)
#define PUBLISH_INTERVAL 60000     // Intervalle publication (ms)  
#define RTC_SYNC_INTERVAL 5000     // Intervalle sync RTC (ms)

// Réseau
#define LOCAL_UDP_PORT 5354        // Port UDP local
#define MDNS_PORT 5353             // Port mDNS standard

// Messages
#define HEARTBEAT_MESSAGE_FORMAT "%s vous dit bonjour. Il est %s"
#define DEFAULT_TIME_STRING "--:--:--"
```

### Macros de Debug

```cpp
#if DEBUG
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)  
  #define DEBUG_PRINTF(x, y) Serial.print(x); Serial.println(y)
  #define DEBUG_BEGIN(x) Serial.begin(x); while (!Serial) { ; }
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x, y)
  #define DEBUG_BEGIN(x)
#endif
```

---

## Gestion des États

### Machine à États Principale

```text
INIT → WIFI_CONNECTING → WIFI_CONNECTED → RTC_SYNCING → 
RTC_SYNCED → MDNS_SEARCHING → MQTT_FOUND → MQTT_CONNECTING → 
MQTT_CONNECTED → PUBLISHING
```

### Variables d'État

```cpp
bool mqttServerFound = false;    // Serveur MQTT détecté
bool mqttConnected = false;      // Connexion MQTT active
bool rtcInitialized = false;     // RTC synchronisé
```

### Transitions d'États

| État Actuel | Condition | Nouvel État | Action |
|-------------|-----------|-------------|---------|
| INIT | WiFi connecté | WIFI_CONNECTED | Démarrer RTC |
| WIFI_CONNECTED | RTC sync | RTC_SYNCED | Démarrer mDNS |
| RTC_SYNCING | Timeout | MDNS_SEARCHING | Continuer sans RTC |
| MDNS_SEARCHING | Serveur trouvé | MQTT_FOUND | Se connecter |
| MQTT_FOUND | Connexion OK | MQTT_CONNECTED | Démarrer heartbeat |
| MQTT_CONNECTED | Erreur | MDNS_SEARCHING | Recommencer |

---

## Messages et Protocoles

### Format des Paquets mDNS

#### Requête mDNS (Sortante)

```text
Header (12 bytes):
  Transaction ID: 0x0000
  Flags: 0x0000 (Standard Query)
  Questions: 0x0001
  Answer RRs: 0x0000
  Authority RRs: 0x0000
  Additional RRs: 0x0000

Query:
  Name: _mqtt._tcp.local. (encodé)
  Type: PTR (0x000C)
  Class: IN (0x0001)
```

#### Réponse mDNS (Entrante)

- Flag Response: 0x8000
- Contient le nom du service recherché
- Peut être dans différents formats d'encodage

### Format des Messages MQTT

#### Topic

- Configurable via `MQTT_TOPIC`
- Défaut: `/arduino`

#### Payload

- Template: `HEARTBEAT_MESSAGE_FORMAT`
- Variables: IP locale + timestamp RTC
- Exemple: `"192.168.1.100 vous dit bonjour. Il est 14:35:22"`

---

## Gestion d'Erreurs

### Stratégies de Récupération

#### Erreurs WiFi

- **Symptôme**: `WiFi.status() != WL_CONNECTED`
- **Action**: Retry automatique dans `connectToWiFi()`
- **Timeout**: Aucun (persistant)

#### Erreurs mDNS

- **Symptôme**: Aucune réponse reçue
- **Action**: Renvoi périodique des requêtes
- **Timeout**: `SEARCH_INTERVAL`

#### Erreurs MQTT

- **Symptôme**: `mqttClient.state() != 0`
- **Action**: Reset du flag `mqttServerFound` et recommence mDNS
- **Codes**: Tous les codes d'erreur MQTT gérés et affichés

#### Erreurs RTC

- **Symptôme**: `WiFi.getTime() == 0`
- **Action**: Retry périodique, fallback sur `DEFAULT_TIME_STRING`
- **Impact**: Système continue de fonctionner

### Messages de Diagnostic

Tous les messages d'erreur incluent:

- Description claire du problème
- Code d'erreur si applicable  
- Action de récupération entreprise
- État du système après récupération

---

## Optimisations

### Optimisations Mémoire

#### Utilisation de `snprintf`

```cpp
// ❌ Évité: Fragmentation de heap
String message = ip + " dit bonjour à " + time;

// ✅ Recommandé: Buffer statique
char message[100];
snprintf(message, sizeof(message), "%s dit bonjour à %s", ip, time);
```

#### Macros de Debug Conditionnelles

- Code debug entièrement éliminé si `DEBUG = false`
- Économie de mémoire flash et RAM significative

### Optimisations CPU

#### Gestion Non-Bloquante

- Aucun `delay()` long dans `loop()`
- Utilisation de `millis()` pour timing
- Architecture basée sur les états

#### Buffers Fixes

```cpp
byte packetBuffer[512];  // Buffer statique pour UDP
char timeStr[10];        // Buffer pour timestamp
char ipStr[16];          // Buffer pour IP
```

### Optimisations Réseau

#### Réutilisation des Connexions

- Client MQTT persistant
- Pas de reconnexion inutile
- Test de connectivité TCP avant MQTT

#### Intervalles Adaptatifs

- Recherche mDNS moins fréquente après découverte
- Publication heartbeat configurabe selon usage
- Sync RTC espacée après succès initial

---

## API de Test et Debug

### Fonctions de Test (Mode Debug uniquement)

```cpp
// Test de connectivité réseau
bool testNetworkConnectivity() {
  return WiFi.status() == WL_CONNECTED;
}

// Test de découverte mDNS  
bool testMDNSDiscovery() {
  searchForMQTTServer();
  delay(5000);
  return mqttServerFound;
}

// Test de connexion MQTT
bool testMQTTConnection() {
  if (!mqttServerFound) return false;
  connectToMQTT();
  return mqttConnected;
}

// Test de publication
bool testMQTTPublish() {
  if (!mqttConnected) return false;
  publishHeartbeat();
  return mqttClient.connected();
}
```

### Commandes de Debug Série

Commandes disponibles en mode debug via Serial:

```text
INFO     - Affiche l'état complet du système
RESTART  - Redémarre la découverte mDNS
PUBLISH  - Force une publication immédiate
TIME     - Affiche l'heure RTC actuelle
WIFI     - Affiche les infos de connexion WiFi
MQTT     - Affiche l'état de la connexion MQTT
```

---

*Documentation générée automatiquement le 24 juin 2025*  
*Version: 1.0.0*  
*Plateforme: Arduino MKR WiFi 1010*
