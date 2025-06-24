# 🚀 Guide Rapide - Mise en Production

## Déploiement Express (5 minutes)

### 1. Préparation

```bash
# Cloner et basculer sur production
git clone https://github.com/dessyd/Arduino-mDNS-UDP.git
cd Arduino-mDNS-UDP
git checkout main  # Version production

# Configurer les credentials WiFi
cp arduino_secrets.h.example arduino_secrets.h
nano arduino_secrets.h  # Éditer avec vos paramètres
```

### 2. Validation Configuration

```bash
# Vérifier configuration production
grep "DEBUG false" config.h           # ✅ Doit retourner une ligne
grep "SEARCH_INTERVAL 60000" config.h # ✅ Intervalle 1 minute
grep "PUBLISH_INTERVAL 300000" config.h # ✅ Intervalle 5 minutes
```

### 3. Déploiement Automatique

```bash
# Rendre le script exécutable
chmod +x deploy_production.sh

# Déploiement complet
./deploy_production.sh

# Ou avec options spécifiques
./deploy_production.sh -p /dev/ttyUSB0 --no-monitor
```

### 4. Validation Post-Production

```bash
# Script de validation automatique
python3 production_validation.py --monitor-time 120

# Ou surveillance manuelle MQTT
mosquitto_sub -h votre-broker -t "/arduino" -v
```

## Checklist Déploiement

### ✅ Pré-Déploiement

- [ ] Arduino MKR 1010 connecté et détecté
- [ ] Réseau WiFi 2.4GHz accessible
- [ ] Broker MQTT opérationnel sur le réseau
- [ ] Service mDNS configuré sur le broker
- [ ] Credentials WiFi corrects dans `arduino_secrets.h`
- [ ] Configuration production validée (`DEBUG false`)

### ✅ Déploiement

- [ ] Compilation réussie sans warnings
- [ ] Upload vérifié avec succès
- [ ] Arduino redémarre correctement
- [ ] Pas de messages debug à la console série

### ✅ Post-Déploiement

- [ ] Messages MQTT reçus dans les 5 minutes
- [ ] Consommation électrique < 50mA
- [ ] Pas de reconnexions WiFi fréquentes
- [ ] Timestamp RTC synchronisé
- [ ] Intervalle de publication respecté (5 minutes)

## Messages MQTT Attendus

### Format Production

```text
Topic: /arduino
Message: "Device 192.168.1.100 online at 14:35:22"
```

### Fréquence

- **Première publication** : < 2 minutes après démarrage
- **Publications suivantes** : Toutes les 5 minutes
- **Après reconnexion** : < 1 minute

## Surveillance Long Terme

### Monitoring MQTT

```bash
# Surveillance continue avec log
mosquitto_sub -h broker -t "/arduino" | while read msg; do
  echo "$(date): $msg" >> arduino_production.log
done &
```

### Métriques Clés

| Métrique | Seuil Normal | Alerte Si |
|----------|--------------|-----------|
| Intervalle publication | 5 ± 1 min | > 7 min ou < 3 min |
| Reconnexions WiFi | < 1/jour | > 5/jour |
| Consommation | 35-45mA | > 60mA |
| Uptime | > 99% | < 95% |

### Alertes Recommandées

```bash
# Script simple d'alerte (crontab)
#!/bin/bash
last_msg=$(tail -1 arduino_production.log | cut -d: -f1-2)
now=$(date "+%Y-%m-%d %H:%M")
diff=$(($(date -d "$now" +%s) - $(date -d "$last_msg" +%s)))

if [ $diff -gt 600 ]; then  # Plus de 10 minutes
  echo "ALERTE: Arduino silencieux depuis $((diff/60)) minutes" | mail admin@example.com
fi
```

## Dépannage Express

### Problème: Pas de messages MQTT

```bash
# Tests rapides
ping arduino_ip                    # Test connectivité
avahi-browse -t _mqtt._tcp         # Test mDNS
mosquitto_pub -h broker -t "/test" -m "test"  # Test broker
```

### Problème: Consommation élevée

```bash
# Vérifications
grep "DEBUG false" config.h        # Debug désactivé ?
arduino-cli monitor -p /dev/ttyACM0 # Messages debug visibles ?
```

### Problème: Reconnexions fréquentes

```bash
# Améliorer signal WiFi
iwconfig wlan0                     # Force signal
sudo iwconfig wlan0 txpower 20     # Augmenter puissance
```

## Maintenance

### Hebdomadaire

- Vérifier logs de surveillance
- Contrôler la consommation électrique
- Valider les publications MQTT

### Mensuelle

- Test complet avec `production_validation.py`
- Vérification mises à jour firmware
- Nettoyage logs anciens

### Mise à Jour

```bash
# Backup avant mise à jour
cp arduino_secrets.h arduino_secrets.h.backup
cp config.h config.h.backup

# Mise à jour code
git pull origin main
./deploy_production.sh

# Validation post-mise à jour
python3 production_validation.py
```

## Support

### Logs Utiles

```bash
# Logs système
journalctl -u avahi-daemon
systemctl status mosquitto

# Logs Arduino (si debug temporaire)
arduino-cli monitor -p /dev/ttyACM0 > arduino_debug.log
```

### Contact Support

- **Issues GitHub** : Problèmes techniques
- **Discussions** : Questions générales
- **Documentation** : [TROUBLESHOOTING.md](TROUBLESHOOTING.md)

---

## 🎯 Objectif Production

**Système autonome, fiable et économe en énergie**

✅ Consommation optimisée  
✅ Communications stables  
✅ Monitoring intégré  
✅ Maintenance minimale

---

*Guide express mis à jour le 24 juin 2025*
