#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <RTCZero.h>
#include "arduino_secrets.h"

// Configuration réseau
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

// Configuration UDP et mDNS
WiFiUDP udp;
const int MDNS_PORT = 5353;
IPAddress mdnsMulticastIP(224, 0, 0, 251);
const int LOCAL_UDP_PORT = 5354;

// Configuration MQTT
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
IPAddress mqttServerIP;
int mqttServerPort = 1883;
bool mqttServerFound = false;
bool mqttConnected = false;

// Configuration RTC
RTCZero rtc;
bool rtcInitialized = false;

// Configuration timing
unsigned long lastSearchTime = 0;
unsigned long lastPublishTime = 0;
const unsigned long SEARCH_INTERVAL = 30000;  // Recherche toutes les 30 secondes si pas trouvé
const unsigned long PUBLISH_INTERVAL = 60000; // Publication toutes les minutes

// Buffer pour les paquets
byte packetBuffer[512];

void setup()
{
  Serial.begin(9600);
  while (!Serial)
  {
    ; // Attendre la connexion série
  }

  Serial.println("Démarrage du client mDNS/MQTT");

  // Connexion WiFi
  connectToWiFi();

  // Initialisation RTC (sans synchronisation)
  initializeRTC();

  // Initialisation UDP
  udp.begin(LOCAL_UDP_PORT);

  Serial.println("Système initialisé");
  Serial.println("Recherche d'un serveur MQTT...");
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
    if (currentTime - lastSearchTime >= SEARCH_INTERVAL)
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
      if (currentTime - lastPublishTime >= PUBLISH_INTERVAL)
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
  Serial.println("Initialisation du module RTC...");
  rtc.begin();
  Serial.println("RTC démarré, synchronisation en cours...");
}

void tryToSyncRTC()
{
  static unsigned long lastSyncAttempt = 0;
  unsigned long currentTime = millis();

  // Essayer de synchroniser toutes les 5 secondes
  if (currentTime - lastSyncAttempt >= 5000)
  {
    lastSyncAttempt = currentTime;

    Serial.println("Tentative de synchronisation RTC avec WiFi.getTime()...");
    unsigned long epochTime = WiFi.getTime();

    if (epochTime != 0)
    {
      // Utiliser setEpoch pour définir directement le timestamp
      rtc.setEpoch(epochTime);

      Serial.println("RTC synchronisé avec WiFi.getTime()!");
      Serial.print("Heure actuelle: ");
      printCurrentTime();

      rtcInitialized = true;
    }
    else
    {
      Serial.println("WiFi.getTime() a retourné 0, nouvelle tentative...");
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
    Serial.println(dateTimeStr);
  }
}

void connectToWiFi()
{
  Serial.print("Connexion au réseau WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connecté!");
  Serial.print("Adresse IP: ");
  Serial.println(WiFi.localIP());
}

void searchForMQTTServer()
{
  Serial.println("\n--- Recherche serveur MQTT ---");

  // Construction du paquet mDNS query
  byte query[64];
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

  // Question: _mqtt._tcp.local.
  query[queryLength++] = 0x05; // "_mqtt"
  query[queryLength++] = '_';
  query[queryLength++] = 'm';
  query[queryLength++] = 'q';
  query[queryLength++] = 't';
  query[queryLength++] = 't';

  query[queryLength++] = 0x04; // "_tcp"
  query[queryLength++] = '_';
  query[queryLength++] = 't';
  query[queryLength++] = 'c';
  query[queryLength++] = 'p';

  query[queryLength++] = 0x05; // "local"
  query[queryLength++] = 'l';
  query[queryLength++] = 'o';
  query[queryLength++] = 'c';
  query[queryLength++] = 'a';
  query[queryLength++] = 'l';

  query[queryLength++] = 0x00; // Fin du nom

  query[queryLength++] = 0x00; // Type PTR
  query[queryLength++] = 0x0C;
  query[queryLength++] = 0x00; // Class IN
  query[queryLength++] = 0x01;

  // Envoi du paquet
  udp.beginPacket(mdnsMulticastIP, MDNS_PORT);
  udp.write(query, queryLength);

  if (udp.endPacket() == 1)
  {
    Serial.println("Requête mDNS envoyée");
  }
  else
  {
    Serial.println("Erreur envoi requête");
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
      Serial.println("\n*** SERVEUR MQTT TROUVÉ! ***");
      Serial.print("IP du serveur: ");
      Serial.println(remoteIP);

      mqttServerIP = remoteIP;
      mqttServerFound = true;

      Serial.println("Arrêt de la recherche mDNS");
      Serial.println("Connexion au serveur MQTT...");
    }
  }
}

void connectToMQTT()
{
  mqttClient.setServer(mqttServerIP, mqttServerPort);

  Serial.print("Connexion MQTT à ");
  Serial.print(mqttServerIP);
  Serial.print(":");
  Serial.println(mqttServerPort);

  String clientId = "Arduino-" + String(WiFi.localIP()[3]);

  if (mqttClient.connect(clientId.c_str()))
  {
    Serial.println("Connexion MQTT réussie!");
    Serial.println("Publication de messages toutes les minutes...");
    mqttConnected = true;
  }
  else
  {
    Serial.print("Erreur connexion MQTT: ");
    Serial.println(mqttClient.state());
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
    strcpy(timeStr, "--:--:--");
  }

  // Formater l'adresse IP avec snprintf
  IPAddress ip = WiFi.localIP();
  char ipStr[16];
  snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

  // Créer le message complet
  char message[100];
  snprintf(message, sizeof(message), "%s vous dit bonjour. Il est %s", ipStr, timeStr);

  Serial.println("\n--- Publication MQTT ---");
  Serial.println("Sujet: /arduino");
  Serial.print("Message: ");
  Serial.println(message);

  if (mqttClient.publish("/arduino", message))
  {
    Serial.println("Message publié avec succès!");
  }
  else
  {
    Serial.println("Erreur publication");
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

  // Recherche de "mqtt" dans le paquet
  for (int i = 0; i < length - 4; i++)
  {
    if (data[i] == 'm' && data[i + 1] == 'q' && data[i + 2] == 't' && data[i + 3] == 't')
    {
      return true;
    }
    // Version encodée avec longueur
    if (data[i] == 0x05 && i + 5 < length)
    {
      if (data[i + 1] == '_' && data[i + 2] == 'm' && data[i + 3] == 'q' &&
          data[i + 4] == 't' && data[i + 5] == 't')
      {
        return true;
      }
    }
  }
  return false;
}