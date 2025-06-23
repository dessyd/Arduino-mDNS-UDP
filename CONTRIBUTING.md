# Guide de Contribution

Merci de votre intérêt pour contribuer à ce projet Arduino mDNS MQTT Client ! 🚀

## 🤝 Comment contribuer

### Signaler un bug

1. Vérifiez que le bug n'a pas déjà été signalé dans les [Issues](../../issues)
2. Créez un nouveau bug report avec :
   - Description claire du problème
   - Étapes pour reproduire
   - Comportement attendu vs observé
   - Version d'Arduino IDE et librairies utilisées
   - Modèle de carte (MKR 1010)
   - Messages série/logs si pertinents

### Proposer une amélioration

1. Vérifiez qu'une suggestion similaire n'existe pas
2. Créez une issue "Feature Request" avec :
   - Description de la fonctionnalité souhaitée
   - Justification / cas d'usage
   - Suggestions d'implémentation si possible

### Contribuer au code

1. **Fork** le repository
2. Créez une **branche** pour votre fonctionnalité (`git checkout -b feature/ma-nouvelle-fonctionnalite`)
3. **Committez** vos changements (`git commit -m 'Ajout: nouvelle fonctionnalité'`)
4. **Push** vers la branche (`git push origin feature/ma-nouvelle-fonctionnalite`)
5. Ouvrez une **Pull Request**

## 📝 Standards de code

### Style de code

- **Indentation** : 2 espaces
- **Noms de variables** : camelCase (`mqttServerFound`)
- **Noms de constantes** : UPPER_CASE (`SEARCH_INTERVAL`)
- **Noms de fonctions** : camelCase (`connectToWiFi()`)
- **Commentaires** : En français, explicites

### Bonnes pratiques

- Code auto-documenté avec commentaires pertinents
- Gestion d'erreurs robuste
- Messages Serial informatifs pour le debug
- Éviter les `delay()` bloquants dans loop()
- Utiliser `snprintf` plutôt que String concatenation

### Tests

- Tester sur hardware réel (MKR 1010)
- Vérifier le comportement en cas de perte WiFi/MQTT
- Tester la découverte mDNS avec différents brokers
- Valider la synchronisation RTC

## ⚙️ Configuration développement

### Prérequis

- Arduino IDE 2.x ou Arduino CLI
- Arduino MKR WiFi 1010
- Accès à un réseau WiFi 2.4GHz
- Broker MQTT (Mosquitto, Home Assistant, etc.)

### Setup

```bash
# Cloner le repository
git clone https://github.com/[username]/Arduino-mDNS-UDP.git
cd Arduino-mDNS-UDP

# Copier le fichier de configuration
cp arduino_secrets.h.example arduino_secrets.h

# Éditer avec vos paramètres WiFi
nano arduino_secrets.h
```

### Librairies requises

- WiFiNINA (incluse)
- RTCZero (incluse)  
- PubSubClient (à installer)

## 📋 Checklist Pull Request

Avant de soumettre votre PR, assurez-vous que :

- [ ] Le code compile sans erreurs ni warnings
- [ ] Les tests sur hardware passent
- [ ] La documentation est mise à jour si nécessaire
- [ ] Le style de code est respecté
- [ ] Les nouveaux fichiers respectent le .gitignore
- [ ] Aucun secret/credential dans le code

## 🐛 Debug et logs

### Messages série utiles

```cpp
Serial.println("DEBUG: État de la connexion MQTT");
Serial.print("WiFi status: ");
Serial.println(WiFi.status());
```

### Tests courants

1. **Test de découverte mDNS** : Vérifier la détection de brokers
2. **Test de reconnexion** : Simuler perte/rétablissement réseau  
3. **Test RTC** : Vérifier synchronisation temporelle
4. **Test de publication** : Valider les messages MQTT

## 🎯 Roadmap

Fonctionnalités souhaitées pour les futures versions :

- [ ] Support de l'authentification MQTT
- [ ] Configuration via WiFi Manager
- [ ] Interface web de monitoring
- [ ] Support de multiples topics
- [ ] Gestion de la qualité de service (QoS)
- [ ] Métriques système (mémoire, uptime)

## 🔍 Communication

- **Discussions** : Utilisez les
  [Discussions GitHub](../../discussions)
- **Questions** : Créez une issue avec le label "question"
- **Bugs** : Issues avec reproduction steps détaillées

## 📜 Code de conduite

Ce projet adhère au [Contributor Covenant](https://www.contributor-covenant.org/).
 En participant, vous vous engagez à maintenir un environnement accueillant et respectueux.

---

Merci pour votre contribution ! 🙏
