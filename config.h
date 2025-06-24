// Configuration générale du projet Arduino mDNS MQTT Client
// Ce fichier contient les paramètres non-confidentiels du projet

#ifndef CONFIG_H
#define CONFIG_H

// ===============================
// Configuration Debug
// ===============================

// Activer/désactiver les messages de debug
#ifndef DEBUG
#define DEBUG true  // Mettre à false pour désactiver tous les messages série
#endif

// ===============================
// Configuration MQTT
// ===============================

// Topic de publication MQTT
#ifndef MQTT_TOPIC
#define MQTT_TOPIC "/arduino"
#endif

// Port MQTT par défaut
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif

// Client ID MQTT (sera complété par l'IP)
#ifndef MQTT_CLIENT_PREFIX
#define MQTT_CLIENT_PREFIX "Arduino"
#endif

// ===============================
// Configuration mDNS - Service Discovery
// ===============================

// Type de service à rechercher
// Options disponibles: "mqtt", "mqtts", "mosquitto"
#ifndef MDNS_SERVICE_TYPE
#define MDNS_SERVICE_TYPE "mosquitto"
#endif

// Protocol pour le service mDNS
#ifndef MDNS_PROTOCOL
#define MDNS_PROTOCOL "tcp"
#endif

// Domaine mDNS
#ifndef MDNS_DOMAIN
#define MDNS_DOMAIN "local"
#endif

// ===============================
// Configuration Timing
// ===============================

// Intervalle de recherche mDNS (millisecondes)
#ifndef SEARCH_INTERVAL
#define SEARCH_INTERVAL 30000 // 30 secondes
#endif

// Intervalle de publication MQTT (millisecondes)
#ifndef PUBLISH_INTERVAL
#define PUBLISH_INTERVAL 60000 // 1 minute
#endif

// Intervalle de tentative de synchronisation RTC (millisecondes)
#ifndef RTC_SYNC_INTERVAL
#define RTC_SYNC_INTERVAL 5000 // 5 secondes
#endif

// ===============================
// Configuration Réseau
// ===============================

// Port UDP local pour mDNS
#ifndef LOCAL_UDP_PORT
#define LOCAL_UDP_PORT 5354
#endif

// Port mDNS standard
#ifndef MDNS_PORT
#define MDNS_PORT 5353
#endif

// ===============================
// Configuration Messages
// ===============================

// Format du message de heartbeat
// Placeholders: %s = IP, %s = timestamp
#ifndef HEARTBEAT_MESSAGE_FORMAT
#define HEARTBEAT_MESSAGE_FORMAT "%s vous dit bonjour. Il est %s"
#endif

// Message par défaut si RTC non synchronisé
#ifndef DEFAULT_TIME_STRING
#define DEFAULT_TIME_STRING "--:--:--"
#endif

#endif // CONFIG_H
