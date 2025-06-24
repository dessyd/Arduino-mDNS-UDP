#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <RTCZero.h>
#include "arduino_secrets.h"
#include "config.h"

// Macros pour les messages de debug conditionnels
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

// Configuration réseau
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

// Configuration UDP et mDNS
WiFiUDP udp;
const int MDNS_PORT_CONST = MDNS_PORT;
IPAddress mdnsMulticastIP(224, 0, 0, 251);
const int LOCAL_UDP_PORT_CONST = LOCAL_UDP_PORT;

// Configuration MQTT
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
IPAddress mqttServerIP;
int mqttServerPort = MQTT_PORT;
const char* mqttTopic = MQTT_TOPIC;

bool mqttServerFound = false;
bool mqttConnected = false;

// Configuration RTC
RTCZero rtc;
bool rtcInitialized = false;

// Configuration timing
unsigned long lastSearchTime = 0;
unsigned long lastPublishTime = 0;
const unsigned long SEARCH_INTERVAL_CONST = SEARCH_INTERVAL;
const unsigned long PUBLISH_INTERVAL_CONST = PUBLISH_INTERVAL;
const unsigned long RTC_SYNC_INTERVAL_CONST = RTC_SYNC_INTERVAL;

// Buffer pour les paquets
byte packetBuffer[512];

// Fonction pour construire le nom du service mDNS
String buildMDNSServiceName() {
  String serviceName = "_";
  serviceName += MDNS_SERVICE_TYPE;
  serviceName += "._";
  serviceName += MDNS_PROTOCOL;
  serviceName += ".";
  serviceName += MDNS_DOMAIN;
  serviceName += ".";
  return serviceName;
}

// Fonction pour construire la partie query du paquet mDNS
void buildMDNSQuery(byte* query, int* queryLength, String serviceName) {
  // Diviser le nom du service en labels
  int start = 0;
  int end = serviceName.indexOf('.', start);
  
  while (end != -1) {
    String label = serviceName.substring(start, end);
    if (label.length() > 0) {
      query[(*queryLength)++] = label.length();
      for (int i = 0; i < label.length(); i++) {
        query[(*queryLength)++] = label.charAt(i);
      }
    }
    start = end + 1;
    end = serviceName.indexOf('.', start);
  }
  
  // Fin du nom
  query[(*queryLength)++] = 0x00;
}

void setup()
{
  DEBUG_BEGIN(9600);

  DEBUG_PRINTLN("Démarrage du client mDNS/MQTT");
  DEBUG_PRINTLN("Configuration:");
  DEBUG_PRINTF("  Service recherché: ", buildMDNSServiceName());
  DEBUG_PRINTF("  Topic MQTT: ", mqttTopic);
  DEBUG_PRINTF("  Port MQTT: ", mqttServerPort);

  // Connexion WiFi
  connectToWiFi();

  // Initialisation RTC (sans synchronisation)
  initializeRTC();

  // Initialisation UDP
  udp.begin(LOCAL_UDP_PORT_CONST);

  DEBUG_PRINTLN("Système initialisé");
  DEBUG_PRINTLN("Recherche d'un serveur MQTT...");
}

void loop()
{
  unsigned long currentTime = millis();

  // Vérifier si le RTC n'est pas encore synchronisé
  if (!rtcInitialized)
  {
    tryToSyncRTC();
  }

  if (!mqttServerFound)
  {
    // Rechercher un serveur MQTT
    if (currentTime - lastSearchTime >= SEARCH_INTERVAL_CONST)
    {
      searchForMQTTServer();
      lastSearchTime = currentTime;
    }
    // Écouter les réponses mDNS
    listenForMDNSResponses();
  }
  else
  {
    // Serveur MQTT trouvé, maintenir la connexion et publier
    if (!mqttConnected)
    {
      connectToMQTT();
    }
    else
    {
      // Publier un message toutes les minutes
      if (currentTime - lastPublishTime >= PUBLISH_INTERVAL_CONST)
      {
        publishHeartbeat();
        lastPublishTime = currentTime;
      }
      // Maintenir la connexion MQTT
      mqttClient.loop();
    }
  }

  delay(100);
}

void initializeRTC()
{
  DEBUG_PRINTLN("Initialisation du module RTC...");
  rtc.begin();
  DEBUG_PRINTLN("RTC démarré, synchronisation en cours...");
}

void tryToSyncRTC()
{
  static unsigned long lastSyncAttempt = 0;
  unsigned long currentTime = millis();

  // Essayer de synchroniser toutes les 5 secondes
  if (currentTime - lastSyncAttempt >= RTC_SYNC_INTERVAL_CONST)
  {
    lastSyncAttempt = currentTime;

    DEBUG_PRINTLN("Tentative de synchronisation RTC avec WiFi.getTime()...");
    unsigned long epochTime = WiFi.getTime();

    if (epochTime != 0)
    {
      // Utiliser setEpoch pour définir directement le timestamp
      rtc.setEpoch(epochTime);

      rtcInitialized = true;

      DEBUG_PRINTLN("RTC synchronisé avec WiFi.getTime()!");
      DEBUG_PRINT("Heure actuelle: ");
      printCurrentTime();
    }
    else
    {
      DEBUG_PRINTLN("WiFi.getTime() a retourné 0, nouvelle tentative...");
    }
  }
}

void printCurrentTime()
{
  if (rtcInitialized)
  {
    char dateTimeStr[20];
    snprintf(dateTimeStr, sizeof(dateTimeStr), "%02d/%02d/20%02d %02d:%02d:%02d",
             rtc.getDay(), rtc.getMonth(), rtc.getYear(),
             rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());
    DEBUG_PRINTLN(dateTimeStr);
  }
}

void connectToWiFi()
{
  DEBUG_PRINTF("Connexion au réseau WiFi: ", ssid);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    DEBUG_PRINT(".");
  }

  DEBUG_PRINTLN("");
  DEBUG_PRINTLN("WiFi connecté!");
  DEBUG_PRINTF("Adresse IP: ", WiFi.localIP());
}

void searchForMQTTServer()
{
  String serviceName = buildMDNSServiceName();
  DEBUG_PRINTLN("\n--- Recherche serveur ---");
  DEBUG_PRINTF("Service: ", serviceName);

  // Construction du paquet mDNS query
  byte query[128];  // Augmenté pour les noms plus longs
  int queryLength = 0;

  // Header mDNS
  query[queryLength++] = 0x00; // Transaction ID
  query[queryLength++] = 0x00;
  query[queryLength++] = 0x00; // Flags - Standard query
  query[queryLength++] = 0x00;
  query[queryLength++] = 0x00; // Questions
  query[queryLength++] = 0x01; // 1 question
  query[queryLength++] = 0x00; // Answer RRs
  query[queryLength++] = 0x00;
  query[queryLength++] = 0x00; // Authority RRs
  query[queryLength++] = 0x00;
  query[queryLength++] = 0x00; // Additional RRs
  query[queryLength++] = 0x00;

  // Construire le nom du service dynamiquement
  buildMDNSQuery(query, &queryLength, serviceName);

  // Type PTR et Class IN
  query[queryLength++] = 0x00; // Type PTR
  query[queryLength++] = 0x0C;
  query[queryLength++] = 0x00; // Class IN
  query[queryLength++] = 0x01;

  // Envoi du paquet
  udp.beginPacket(mdnsMulticastIP, MDNS_PORT_CONST);
  udp.write(query, queryLength);

  if (udp.endPacket() == 1)
  {
    DEBUG_PRINTLN("Requête mDNS envoyée");
  }
  else
  {
    DEBUG_PRINTLN("Erreur envoi requête");
  }
}

void listenForMDNSResponses()
{
  int packetSize = udp.parsePacket();

  if (packetSize > 0)
  {
    IPAddress remoteIP = udp.remoteIP();

    // Lire le paquet
    int len = udp.read(packetBuffer, sizeof(packetBuffer));

    if (len > 0 && isMQTTResponse(packetBuffer, len))
    {
      DEBUG_PRINTLN("\n*** SERVEUR MQTT TROUVÉ! ***");
      DEBUG_PRINTF("IP du serveur: ", remoteIP);

      mqttServerIP = remoteIP;
      mqttServerFound = true;

      DEBUG_PRINTLN("Arrêt de la recherche mDNS");
      DEBUG_PRINTLN("Connexion au serveur MQTT...");
    }
  }
}

void connectToMQTT()
{
  mqttClient.setServer(mqttServerIP, mqttServerPort);

  DEBUG_PRINTF("Connexion MQTT à ", mqttServerIP);
  DEBUG_PRINTF(":", mqttServerPort);
  
  // Test de connectivité TCP avant MQTT
  WiFiClient testClient;
  DEBUG_PRINT("Test de connectivité TCP...");
  if (testClient.connect(mqttServerIP, mqttServerPort)) {
    DEBUG_PRINTLN(" OK");
    testClient.stop();
  } else {
    DEBUG_PRINTLN(" ÉCHEC - Serveur inaccessible!");
    mqttServerFound = false;
    return;
  }

  String clientId = "Arduino-" + String(WiFi.localIP()[3]);
  DEBUG_PRINTF("Client ID: ", clientId);

  if (mqttClient.connect(clientId.c_str()))
  {
    DEBUG_PRINTLN("Connexion MQTT réussie!");
    DEBUG_PRINTLN("Publication de messages toutes les minutes...");
    mqttConnected = true;
  }
  else
  {
    DEBUG_PRINTF("Erreur connexion MQTT: ", mqttClient.state());
    
    // Décodage des erreurs MQTT
    switch(mqttClient.state()) {
      case -4: DEBUG_PRINTLN("  -> MQTT_CONNECTION_TIMEOUT"); break;
      case -3: DEBUG_PRINTLN("  -> MQTT_CONNECTION_LOST"); break;
      case -2: DEBUG_PRINTLN("  -> MQTT_CONNECT_FAILED (TCP échec)"); break;
      case -1: DEBUG_PRINTLN("  -> MQTT_DISCONNECTED"); break;
      case 1: DEBUG_PRINTLN("  -> MQTT_CONNECT_BAD_PROTOCOL"); break;
      case 2: DEBUG_PRINTLN("  -> MQTT_CONNECT_BAD_CLIENT_ID"); break;
      case 3: DEBUG_PRINTLN("  -> MQTT_CONNECT_UNAVAILABLE"); break;
      case 4: DEBUG_PRINTLN("  -> MQTT_CONNECT_BAD_CREDENTIALS"); break;
      case 5: DEBUG_PRINTLN("  -> MQTT_CONNECT_UNAUTHORIZED"); break;
      default: DEBUG_PRINTLN("  -> Erreur inconnue"); break;
    }
    
    // Recommencer la recherche si la connexion échoue
    mqttServerFound = false;
  }
}

void publishHeartbeat()
{
  if (!mqttClient.connected())
  {
    mqttConnected = false;
    return;
  }

  // Créer le timestamp avec snprintf
  char timeStr[10];
  if (rtcInitialized)
  {
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
             rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());
  }
  else
  {
    strcpy(timeStr, DEFAULT_TIME_STRING);
  }

  // Formater l'adresse IP avec snprintf
  IPAddress ip = WiFi.localIP();
  char ipStr[16];
  snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

  // Créer le message complet avec le format configurable
  char message[100];
  snprintf(message, sizeof(message), HEARTBEAT_MESSAGE_FORMAT, ipStr, timeStr);

  DEBUG_PRINTLN("\n--- Publication MQTT ---");
  DEBUG_PRINTF("Sujet: ", mqttTopic);
  DEBUG_PRINTF("Message: ", message);

  if (mqttClient.publish(mqttTopic, message))
  {
    DEBUG_PRINTLN("Message publié avec succès!");
  }
  else
  {
    DEBUG_PRINTLN("Erreur publication");
  }
}

bool isMQTTResponse(byte *data, int length)
{
  // Vérifier si c'est une réponse (pas une requête)
  if (length < 12)
    return false;

  uint16_t flags = (data[2] << 8) | data[3];
  bool isResponse = (flags & 0x8000) != 0;

  if (!isResponse)
    return false;

  // Recherche du type de service configuré dans le paquet
  String serviceType = MDNS_SERVICE_TYPE;
  
  for (int i = 0; i < length - serviceType.length(); i++)
  {
    // Vérifier la correspondance avec le type de service
    bool match = true;
    for (int j = 0; j < serviceType.length(); j++) {
      if (data[i + j] != serviceType.charAt(j)) {
        match = false;
        break;
      }
    }
    if (match) return true;
    
    // Vérifier aussi la version encodée avec longueur
    if (data[i] == serviceType.length() + 1 && i + serviceType.length() + 1 < length)
    {
      if (data[i + 1] == '_') {
        bool encodedMatch = true;
        for (int j = 0; j < serviceType.length(); j++) {
          if (data[i + 2 + j] != serviceType.charAt(j)) {
            encodedMatch = false;
            break;
          }
        }
        if (encodedMatch) return true;
      }
    }
  }
  return false;
}