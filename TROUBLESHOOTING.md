# 🛠️ Guide de Dépannage Avancé - Arduino mDNS MQTT Client

## 📋 Table des Matières

- [Diagnostic Rapide](#diagnostic-rapide)
- [Problèmes WiFi](#problèmes-wifi)
- [Problèmes mDNS](#problèmes-mdns)
- [Problèmes MQTT](#problèmes-mqtt)
- [Problèmes RTC](#problèmes-rtc)
- [Problèmes de Performance](#problèmes-de-performance)
- [Outils de Diagnostic](#outils-de-diagnostic)
- [FAQ](#faq)

---

## Diagnostic Rapide

### 🚦 Codes de Statut LED (si implémentés)

```cpp
// Codes couleur LED (à ajouter au code principal si nécessaire)
LED_ROUGE_FIXE      = WiFi non connecté
LED_ROUGE_CLIGNOTANT = Tentative connexion WiFi
LED_ORANGE_FIXE     = WiFi OK, recherche mDNS
LED_ORANGE_CLIGNOTANT = mDNS en cours
LED_BLEU_FIXE       = MQTT trouvé, connexion en cours
LED_BLEU_CLIGNOTANT = Connexion MQTT échouée
LED_VERT_FIXE       = Tout fonctionne normalement
LED_VERT_CLIGNOTANT = Publication MQTT en cours
```

### 🔍 Messages Série de Diagnostic

| Message | Signification | Action |
|---------|---------------|---------|
| `Connexion au réseau WiFi: ...` | Tentative connexion | Normal |
| `WiFi connecté!` | Connexion WiFi OK | ✅ |
| `RTC synchronisé` | Horloge OK | ✅ |
| `SERVEUR MQTT TROUVÉ!` | mDNS réussi | ✅ |
| `Connexion MQTT réussie!` | MQTT OK | ✅ |
| `Message publié avec succès!` | Publication OK | ✅ |
| `Erreur connexion MQTT: -2` | Problème TCP | ⚠️ |
| `Recomencer la recherche` | Reset après erreur | 🔄 |

---

## Problèmes WiFi

### ❌ Symptôme: Connexion WiFi échoue

#### Causes Possibles et Solutions

#### 1. Identifiants incorrects

```cpp
// Vérifier arduino_secrets.h
#define SECRET_SSID "ExactNomReseau"     // ⚠️ Sensible à la casse
#define SECRET_PASS "MotDePasseExact"   // ⚠️ Caractères spéciaux
```

**Diagnostic:**

```bash
# Lister réseaux disponibles
nmcli dev wifi list
# ou
iwlist scan | grep ESSID
```

#### 2. Réseau 5GHz au lieu de 2.4GHz

```text
⚠️ Arduino MKR 1010 supporte SEULEMENT 2.4GHz
✅ Solution: Configurer SSID séparé 2.4GHz sur routeur
```

#### 3. Signal trop faible

```cpp
// Ajouter dans le code pour diagnostic
DEBUG_PRINTF("Signal strength: ", WiFi.RSSI());

// Valeurs de référence:
// > -50 dBm: Excellent
// > -70 dBm: Bon  
// > -80 dBm: Acceptable
// < -80 dBm: Problématique
```

**Solutions:**

- Rapprocher Arduino du routeur
- Utiliser antenne externe
- Réduire interférences (micro-ondes, Bluetooth)

#### 4. Authentification WPA3/WPS

```text
❌ WPA3: Non supporté par Arduino
❌ WPS: Non supporté  
✅ WPA2-PSK: Supporté
✅ Réseau ouvert: Supporté
```

---

## Problèmes mDNS

### ❌ Symptôme: Serveur MQTT non trouvé

#### Diagnostic mDNS

#### 1. Vérifier services mDNS disponibles

```bash
# Linux/macOS
avahi-browse -t _mqtt._tcp

# Sortie attendue:
# + eth0 IPv4 Mosquitto MQTT broker on hostname _mqtt._tcp local

# Si aucun service:
sudo systemctl status avahi-daemon
brew services restart avahi  # macOS
```

#### 2. Tester requête mDNS manuelle

```bash
# Envoyer requête mDNS avec dig
dig @224.0.0.251 -p 5353 _mqtt._tcp.local PTR

# Ou avec avahi-resolve
avahi-resolve-host-name mosquitto.local
```

#### 3. Vérifier configuration broker

**Mosquitto:**

```bash
# Créer service mDNS pour Mosquitto
sudo tee /etc/avahi/services/mqtt.service << EOF
<?xml version="1.0" standalone='no'?>
<!DOCTYPE service-group SYSTEM "avahi-service.dtd">
<service-group>
  <name replace-wildcards="yes">Mosquitto MQTT on %h</name>
  <service>
    <type>_mqtt._tcp</type>
    <port>1883</port>
    <txt-record>path=/mqtt</txt-record>
  </service>
</service-group>
EOF

sudo systemctl reload avahi-daemon
```

---

## Problèmes MQTT

### ❌ Symptôme: Connexion MQTT échoue

#### Codes d'Erreur MQTT

| Code | Signification | Diagnostic | Solution |
|------|---------------|------------|----------|
| -4 | `CONNECTION_TIMEOUT` | Réseau lent/instable | Augmenter timeout |
| -3 | `CONNECTION_LOST` | Connexion interrompue | Vérifier réseau |
| -2 | `CONNECT_FAILED` | Échec TCP | Vérifier IP/port |
| -1 | `DISCONNECTED` | Déconnexion normale | Reconnecter |
| 1 | `BAD_PROTOCOL` | Version MQTT incompatible | Vérifier broker |
| 2 | `BAD_CLIENT_ID` | ID client rejeté | Changer client ID |
| 3 | `UNAVAILABLE` | Serveur indisponible | Vérifier broker |
| 4 | `BAD_CREDENTIALS` | Auth échouée | Vérifier user/pass |
| 5 | `UNAUTHORIZED` | Pas autorisé | Vérifier permissions |

#### Solutions Spécifiques

#### 1. Test connectivité TCP

```bash
# Tester port MQTT
telnet broker_ip 1883
# ou
nc -zv broker_ip 1883

# Si échec:
# - Vérifier broker démarré
# - Vérifier pare-feu
# - Vérifier port correct
```

#### 2. Test client MQTT

```bash
# Test avec mosquitto_pub/sub
mosquitto_sub -h broker_ip -t "/test" &
mosquitto_pub -h broker_ip -t "/test" -m "Hello"

# Si auth requise:
mosquitto_pub -h broker_ip -u username -P password -t "/test" -m "Hello"
```

---

## Problèmes RTC

### ❌ Symptôme: RTC non synchronisé

#### Diagnostic

#### 1. Vérifier connectivité NTP

```cpp
// Ajouter debug WiFi.getTime()
void tryToSyncRTC() {
  // ... code existant ...
  
  DEBUG_PRINTLN("Tentative WiFi.getTime()...");
  unsigned long epochTime = WiFi.getTime();
  
  DEBUG_PRINTF("Epoch reçu: ", epochTime);
  
  if (epochTime == 0) {
    // Tests supplémentaires
    DEBUG_PRINTF("WiFi status: ", WiFi.status());
    DEBUG_PRINTF("WiFi RSSI: ", WiFi.RSSI());
    DEBUG_PRINTF("Gateway: ", WiFi.gatewayIP());
    DEBUG_PRINTF("DNS: ", WiFi.dnsIP());
  }
}
```

#### 2. Test NTP manuel

```bash
# Tester serveurs NTP
ntpdate -q pool.ntp.org
ntpdate -q time.google.com
ntpdate -q time.cloudflare.com

# Vérifier connectivité NTP
telnet pool.ntp.org 123
```

---

## Problèmes de Performance

### ❌ Symptôme: Consommation excessive

#### Diagnostic Consommation

#### 1. Mesure par composant

```cpp
// Ajouter mesures de consommation
void measurePowerConsumption() {
  // Mesure baseline (WiFi off)
  WiFi.end();
  delay(5000);  // Stabilisation
  DEBUG_PRINTLN("Mesure 1: WiFi OFF (baseline)");
  
  // Mesure WiFi seul
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) delay(100);
  delay(5000);
  DEBUG_PRINTLN("Mesure 2: WiFi ON, pas d'activité");
  
  // Mesure avec activité réseau
  udp.begin(LOCAL_UDP_PORT_CONST);
  delay(5000);
  DEBUG_PRINTLN("Mesure 3: WiFi + UDP");
  
  // Mesure avec MQTT
  mqttClient.connect("test-client");
  delay(5000);
  DEBUG_PRINTLN("Mesure 4: WiFi + UDP + MQTT");
}
```

#### 2. Optimisations consommation

```cpp
// Mode basse consommation WiFi
void optimizePowerConsumption() {
  // Réduire puissance TX WiFi
  WiFi.setTxPower(WIFI_POWER_11dBm);  // Au lieu de 20dBm max
  
  // Mode sleep WiFi entre activités
  WiFi.setSleep(WIFI_PS_MIN_MODEM);  // Power saving
  
  // Désactiver LED intégrée si pas nécessaire
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  
  // Optimiser intervalles
  #define SEARCH_INTERVAL 120000    // 2 min au lieu de 30 sec
  #define PUBLISH_INTERVAL 600000   // 10 min au lieu de 1 min
}
```

---

## Outils de Diagnostic

### 🔧 Script de Diagnostic Complet

```python
#!/usr/bin/env python3
# comprehensive_diagnostics.py

import subprocess
import socket
import time
import sys
from datetime import datetime

class ComprehensiveDiagnostics:
    def __init__(self):
        self.results = {}
        
    def test_network_connectivity(self):
        """Test connectivité réseau de base"""
        print("🌐 Test connectivité réseau...")
        
        tests = [
            ("Gateway", self.get_gateway_ip()),
            ("DNS Google", "8.8.8.8"),
            ("Internet", "google.com")
        ]
        
        for name, target in tests:
            if target:
                result = self.ping_test(target)
                self.results[f"connectivity_{name.lower()}"] = result
                status = "✅" if result['success'] else "❌"
                print(f"  {status} {name}: {result['message']}")
    
    def ping_test(self, target, count=3):
        """Test ping vers cible"""
        try:
            result = subprocess.run(
                ['ping', '-c', str(count), target],
                capture_output=True, text=True, timeout=10
            )
            
            if result.returncode == 0:
                return {'success': True, 'message': 'OK'}
            else:
                return {'success': False, 'message': 'Unreachable'}
                
        except Exception as e:
            return {'success': False, 'message': str(e)}
    
    def get_gateway_ip(self):
        """Obtenir IP passerelle"""
        try:
            result = subprocess.run(
                ['ip', 'route', 'show', 'default'],
                capture_output=True, text=True
            )
            
            for line in result.stdout.split('\n'):
                if 'default via' in line:
                    return line.split()[2]
                    
        except:
            return None
    
    def run_all_tests(self):
        """Exécuter tous les tests"""
        print("🚀 Diagnostic complet Arduino mDNS MQTT")
        print("=" * 50)
        print(f"Timestamp: {datetime.now()}")
        
        self.test_network_connectivity()
        
        print("\n📊 Résumé:")
        total_tests = len(self.results)
        passed_tests = sum(1 for v in self.results.values() 
                          if isinstance(v, dict) and v.get('success'))
        
        print(f"  Tests passés: {passed_tests}/{total_tests}")
        
        if passed_tests == total_tests:
            print("  ✅ Tous les tests OK - Prêt pour déploiement Arduino")
        elif passed_tests > total_tests * 0.7:
            print("  ⚠️ La plupart des tests OK - Quelques ajustements nécessaires")
        else:
            print("  ❌ Plusieurs problèmes détectés - Vérifier configuration réseau")

def main():
    diagnostics = ComprehensiveDiagnostics()
    diagnostics.run_all_tests()

if __name__ == "__main__":
    main()
```

---

## FAQ

### ❓ Questions Fréquentes

**Q: Arduino se connecte au WiFi mais ne trouve pas le serveur MQTT**

**R:** Problème mDNS le plus courant. Solutions:

1. Vérifier que le broker annonce le service mDNS: `avahi-browse -t _mqtt._tcp`
2. Essayer service générique: `#define MDNS_SERVICE_TYPE "mqtt"`
3. Configurer IP statique du broker si mDNS ne fonctionne pas

**Q: Messages MQTT publiés mais pas reçus côté broker**

**R:** Vérifications:

1. Topic exact (sensible à la casse): `mosquitto_sub -h localhost -t "/arduino" -v`
2. QoS settings: essayer `retain=true`
3. Permissions broker si authentification activée

**Q: Consommation électrique trop élevée**

**R:** Optimisations:

1. `WiFi.setSleep(WIFI_PS_MIN_MODEM)`
2. Augmenter intervalles de publication
3. Réduire puissance TX: `WiFi.setTxPower(WIFI_POWER_11dBm)`

**Q: RTC ne se synchronise jamais**

**R:** Alternatives:

1. Vérifier connectivité NTP: `ntpdate -q pool.ntp.org`
2. Utiliser uptime comme fallback
3. Implémenter sync NTP manuel si `WiFi.getTime()` échoue

**Q: Déconnexions WiFi fréquentes**

**R:** Solutions:

1. Désactiver power saving: `WiFi.noLowPowerMode()`
2. Configurer IP statique pour éviter problèmes DHCP
3. Ajouter watchdog WiFi avec reconnexion automatique

**Q: Performance dégradée au fil du temps**

**R:** Causes possibles:

1. Fragmentation mémoire → utiliser `snprintf` au lieu de String
2. Buffers qui débordent → augmenter tailles buffers
3. Pas de nettoyage ressources → ajouter cleanup dans error handlers

---

*Guide mis à jour le 24 juin 2025*  
*Version: 1.0*  
*Support: Créer une issue GitHub pour questions spécifiques*
