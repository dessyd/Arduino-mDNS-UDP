// Configuration PRODUCTION du projet Arduino mDNS MQTT Client
// Ce fichier contient les paramètres optimisés pour la production

#ifndef CONFIG_H
#define CONFIG_H

// ===============================
// Configuration Debug
// ===============================

// PRODUCTION: Messages de debug désactivés pour optimiser les performances
#ifndef DEBUG
#define DEBUG false // FALSE pour production - économise mémoire et CPU
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

// Type de service à rechercher - PRODUCTION: Plus générique
// Options disponibles: "mqtt", "mqtts", "mosquitto"
#ifndef MDNS_SERVICE_TYPE
#define MDNS_SERVICE_TYPE "mqtt" // Générique pour compatibilité maximale
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
// Configuration Timing - PRODUCTION
// ===============================

// Intervalle de recherche mDNS (millisecondes) - Optimisé pour production
#ifndef SEARCH_INTERVAL
#define SEARCH_INTERVAL 60000 // 1 minute - Moins fréquent pour économiser bande passante
#endif

// Intervalle de publication MQTT (millisecondes)
#ifndef PUBLISH_INTERVAL
#define PUBLISH_INTERVAL 300000 // 5 minutes - Production normale
#endif

// Intervalle de tentative de synchronisation RTC (millisecondes)
#ifndef RTC_SYNC_INTERVAL
#define RTC_SYNC_INTERVAL 10000 // 10 secondes - Plus espacé
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
// Configuration Messages - PRODUCTION
// ===============================

// Format du message de heartbeat optimisé
// Placeholders: %s = IP, %s = timestamp
#ifndef HEARTBEAT_MESSAGE_FORMAT
#define HEARTBEAT_MESSAGE_FORMAT "Device %s online at %s"
#endif

// Message par défaut si RTC non synchronisé
#ifndef DEFAULT_TIME_STRING
#define DEFAULT_TIME_STRING "N/A"
#endif

// ===============================
// Configuration Monitoring - PRODUCTION
// ===============================

// Intervalle de monitoring système (millisecondes)
#ifndef MONITORING_INTERVAL
#define MONITORING_INTERVAL 3600000 // 1 heure
#endif

// Seuil d'alerte mémoire libre (bytes)
#ifndef LOW_MEMORY_THRESHOLD
#define LOW_MEMORY_THRESHOLD 1024 // 1KB
#endif

#endif // CONFIG_H
