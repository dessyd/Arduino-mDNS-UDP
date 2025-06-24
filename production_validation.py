#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Script de Validation Production - Arduino mDNS MQTT Client
Vérifie le bon fonctionnement du système en production
"""

import time
import sys
import subprocess
import socket
import argparse
from datetime import datetime
import json
import signal
import logging
from typing import Dict, List, Optional, Tuple

# Configuration du logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)

class Colors:
    """Codes couleur ANSI pour l'affichage console"""
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    WHITE = '\033[97m'
    BOLD = '\033[1m'
    END = '\033[0m'

class ProductionValidator:
    """Classe principale pour la validation de production"""
    
    def __init__(self, mqtt_broker: str = None, mqtt_topic: str = "/arduino"):
        self.mqtt_broker = mqtt_broker
        self.mqtt_topic = mqtt_topic
        self.results = {}
        self.start_time = time.time()
        
    def print_header(self):
        """Affiche l'en-tête du script"""
        print(f"{Colors.BLUE}{Colors.BOLD}")
        print("=" * 60)
        print("  VALIDATION PRODUCTION - Arduino mDNS MQTT Client")
        print("=" * 60)
        print(f"{Colors.END}")
        print(f"Démarrage: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print()
    
    def test_network_connectivity(self) -> bool:
        """Test de connectivité réseau de base"""
        print(f"{Colors.CYAN}🌐 Test de connectivité réseau...{Colors.END}")
        
        tests = [
            ("Gateway local", self.get_gateway_ip()),
            ("DNS Google", "8.8.8.8"),
            ("Internet", "google.com")
        ]
        
        all_passed = True
        for name, target in tests:
            if target:
                success, latency = self.ping_test(target)
                status = f"{Colors.GREEN}✅{Colors.END}" if success else f"{Colors.RED}❌{Colors.END}"
                latency_str = f"({latency:.1f}ms)" if latency else ""
                print(f"  {status} {name}: {target} {latency_str}")
                
                if not success:
                    all_passed = False
            else:
                print(f"  {Colors.YELLOW}⚠️{Colors.END} {name}: Non détecté")
        
        self.results['network'] = all_passed
        return all_passed
    
    def test_mdns_services(self) -> bool:
        """Test de découverte des services mDNS"""
        print(f"{Colors.CYAN}🔍 Test de découverte mDNS...{Colors.END}")
        
        try:
            # Utiliser avahi-browse pour chercher les services MQTT
            result = subprocess.run(
                ['avahi-browse', '-t', '_mqtt._tcp', '--resolve', '--parsable'],
                capture_output=True, text=True, timeout=10
            )
            
            if result.returncode == 0 and result.stdout:
                services = []
                for line in result.stdout.strip().split('\n'):
                    if line.startswith('=') and 'IPv4' in line:
                        parts = line.split(';')
                        if len(parts) >= 8:
                            name = parts[3]
                            hostname = parts[6]
                            address = parts[7]
                            port = parts[8]
                            services.append((name, hostname, address, port))
                
                if services:
                    print(f"  {Colors.GREEN}✅{Colors.END} Services MQTT détectés:")
                    for name, hostname, address, port in services:
                        print(f"    - {name} @ {address}:{port}")
                    
                    # Utiliser le premier service trouvé si pas de broker spécifié
                    if not self.mqtt_broker:
                        self.mqtt_broker = services[0][2]  # Adresse IP
                    
                    self.results['mdns'] = True
                    return True
                else:
                    print(f"  {Colors.YELLOW}⚠️{Colors.END} Aucun service MQTT trouvé via mDNS")
            else:
                print(f"  {Colors.RED}❌{Colors.END} Erreur avahi-browse ou aucun service")
        
        except FileNotFoundError:
            print(f"  {Colors.YELLOW}⚠️{Colors.END} avahi-browse non disponible")
        except subprocess.TimeoutExpired:
            print(f"  {Colors.RED}❌{Colors.END} Timeout lors de la recherche mDNS")
        except Exception as e:
            print(f"  {Colors.RED}❌{Colors.END} Erreur: {e}")
        
        self.results['mdns'] = False
        return False
    
    def test_mqtt_connectivity(self) -> bool:
        """Test de connectivité MQTT"""
        print(f"{Colors.CYAN}📡 Test de connectivité MQTT...{Colors.END}")
        
        if not self.mqtt_broker:
            print(f"  {Colors.YELLOW}⚠️{Colors.END} Pas de broker MQTT spécifié")
            self.results['mqtt'] = False
            return False
        
        try:
            # Test de connexion TCP au port MQTT
            sock = socket.create_connection((self.mqtt_broker, 1883), timeout=5)
            sock.close()
            print(f"  {Colors.GREEN}✅{Colors.END} Connexion TCP au broker {self.mqtt_broker}:1883")
            
            # Test avec mosquitto_pub si disponible
            try:
                result = subprocess.run(
                    ['mosquitto_pub', '-h', self.mqtt_broker, '-t', '/test/validation', 
                     '-m', f'Test validation {datetime.now()}', '-q', '0'],
                    capture_output=True, text=True, timeout=10
                )
                
                if result.returncode == 0:
                    print(f"  {Colors.GREEN}✅{Colors.END} Publication MQTT test réussie")
                    self.results['mqtt'] = True
                    return True
                else:
                    print(f"  {Colors.YELLOW}⚠️{Colors.END} Publication MQTT échouée: {result.stderr}")
            
            except FileNotFoundError:
                print(f"  {Colors.YELLOW}⚠️{Colors.END} mosquitto_pub non disponible, test TCP seul")
                self.results['mqtt'] = True
                return True
        
        except Exception as e:
            print(f"  {Colors.RED}❌{Colors.END} Erreur connexion MQTT: {e}")
        
        self.results['mqtt'] = False
        return False
    
    def monitor_mqtt_messages(self, duration: int = 300) -> bool:
        """Surveille les messages MQTT pendant une durée donnée"""
        print(f"{Colors.CYAN}📊 Surveillance MQTT pendant {duration}s...{Colors.END}")
        
        if not self.mqtt_broker:
            print(f"  {Colors.YELLOW}⚠️{Colors.END} Pas de broker MQTT pour surveillance")
            return False
        
        try:
            # Utiliser mosquitto_sub pour écouter
            process = subprocess.Popen(
                ['mosquitto_sub', '-h', self.mqtt_broker, '-t', self.mqtt_topic, '-v'],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
            )
            
            messages_received = 0
            start_monitor = time.time()
            
            print(f"  Écoute sur topic: {self.mqtt_topic}")
            print(f"  Appuyez sur Ctrl+C pour arrêter plus tôt")
            
            try:
                while time.time() - start_monitor < duration:
                    # Vérifier s'il y a des données disponibles
                    process.poll()
                    if process.returncode is not None:
                        break
                    
                    # Lire ligne par ligne avec timeout
                    try:
                        line = process.stdout.readline()
                        if line:
                            messages_received += 1
                            timestamp = datetime.now().strftime('%H:%M:%S')
                            print(f"  [{timestamp}] Message {messages_received}: {line.strip()}")
                    except:
                        pass
                    
                    time.sleep(0.1)
            
            except KeyboardInterrupt:
                print(f"\n  {Colors.YELLOW}Surveillance interrompue par l'utilisateur{Colors.END}")
            
            finally:
                process.terminate()
                process.wait(timeout=5)
            
            elapsed = time.time() - start_monitor
            rate = messages_received / (elapsed / 60) if elapsed > 0 else 0
            
            print(f"\n  {Colors.GREEN}📈{Colors.END} Résultats surveillance:")
            print(f"    Messages reçus: {messages_received}")
            print(f"    Durée: {elapsed:.1f}s")
            print(f"    Taux: {rate:.2f} msg/min")
            
            # Critères de validation
            success = messages_received > 0
            if success:
                print(f"  {Colors.GREEN}✅{Colors.END} Arduino communique correctement")
                
                # Vérifications supplémentaires
                if rate < 0.1:  # Moins d'1 message toutes les 10 minutes
                    print(f"  {Colors.YELLOW}⚠️{Colors.END} Taux de publication faible")
                elif rate > 2:  # Plus de 2 messages par minute
                    print(f"  {Colors.YELLOW}⚠️{Colors.END} Taux de publication élevé")
                else:
                    print(f"  {Colors.GREEN}✅{Colors.END} Taux de publication optimal")
            else:
                print(f"  {Colors.RED}❌{Colors.END} Aucun message reçu de l'Arduino")
            
            self.results['mqtt_monitoring'] = {
                'success': success,
                'messages_count': messages_received,
                'duration': elapsed,
                'rate_per_min': rate
            }
            
            return success
        
        except FileNotFoundError:
            print(f"  {Colors.RED}❌{Colors.END} mosquitto_sub non disponible")
        except Exception as e:
            print(f"  {Colors.RED}❌{Colors.END} Erreur surveillance: {e}")
        
        return False
    
    def test_production_config(self) -> bool:
        """Vérifie la configuration de production"""
        print(f"{Colors.CYAN}⚙️ Vérification configuration production...{Colors.END}")
        
        config_file = "config.h"
        checks_passed = 0
        total_checks = 0
        
        try:
            with open(config_file, 'r') as f:
                content = f.read()
            
            checks = [
                ("DEBUG false", "DEBUG false" in content, "Debug désactivé"),
                ("SEARCH_INTERVAL >= 60000", "60000" in content or "SEARCH_INTERVAL" in content, "Intervalle mDNS optimisé"),
                ("PUBLISH_INTERVAL >= 300000", "300000" in content or "PUBLISH_INTERVAL" in content, "Intervalle publication optimisé"),
                ("Service générique", '"mqtt"' in content, "Service mDNS générique")
            ]
            
            for check_name, condition, description in checks:
                total_checks += 1
                status = f"{Colors.GREEN}✅{Colors.END}" if condition else f"{Colors.RED}❌{Colors.END}"
                print(f"  {status} {description}")
                if condition:
                    checks_passed += 1
            
            success = checks_passed == total_checks
            self.results['config'] = {'passed': checks_passed, 'total': total_checks}
            return success
        
        except FileNotFoundError:
            print(f"  {Colors.RED}❌{Colors.END} Fichier {config_file} non trouvé")
            return False
    
    def ping_test(self, target: str, count: int = 3) -> Tuple[bool, Optional[float]]:
        """Test ping vers une cible"""
        try:
            result = subprocess.run(
                ['ping', '-c', str(count), target],
                capture_output=True, text=True, timeout=10
            )
            
            if result.returncode == 0:
                # Extraire la latency moyenne
                lines = result.stdout.split('\n')
                for line in lines:
                    if 'avg' in line or 'mdev' in line:
                        try:
                            # Format: min/avg/max/mdev = x.x/x.x/x.x/x.x ms
                            parts = line.split('=')[1].strip().split('/')
                            avg_latency = float(parts[1])
                            return True, avg_latency
                        except:
                            pass
                return True, None
            
            return False, None
        
        except Exception:
            return False, None
    
    def get_gateway_ip(self) -> Optional[str]:
        """Obtient l'IP de la passerelle"""
        try:
            result = subprocess.run(
                ['ip', 'route', 'show', 'default'],
                capture_output=True, text=True
            )
            
            for line in result.stdout.split('\n'):
                if 'default via' in line:
                    return line.split()[2]
        except:
            pass
        
        return None
    
    def generate_report(self):
        """Génère un rapport de validation"""
        print(f"\n{Colors.BLUE}{Colors.BOLD}📋 RAPPORT DE VALIDATION PRODUCTION{Colors.END}")
        print("=" * 50)
        
        total_time = time.time() - self.start_time
        print(f"Durée totale: {total_time:.1f}s")
        print(f"Timestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print()
        
        # Résumé des tests
        total_tests = len(self.results)
        passed_tests = sum(1 for result in self.results.values() 
                          if (isinstance(result, bool) and result) or 
                             (isinstance(result, dict) and result.get('success')))
        
        print(f"Tests réalisés: {total_tests}")
        print(f"Tests réussis: {passed_tests}")
        print(f"Taux de réussite: {(passed_tests/total_tests)*100:.1f}%")
        print()
        
        # Détails par test
        for test_name, result in self.results.items():
            if isinstance(result, bool):
                status = f"{Colors.GREEN}✅{Colors.END}" if result else f"{Colors.RED}❌{Colors.END}"
                print(f"{status} {test_name.replace('_', ' ').title()}")
            elif isinstance(result, dict):
                if 'success' in result:
                    status = f"{Colors.GREEN}✅{Colors.END}" if result['success'] else f"{Colors.RED}❌{Colors.END}"
                    print(f"{status} {test_name.replace('_', ' ').title()}")
                elif 'passed' in result and 'total' in result:
                    print(f"📊 {test_name.replace('_', ' ').title()}: {result['passed']}/{result['total']}")
        
        print()
        
        # Recommandations
        if passed_tests == total_tests:
            print(f"{Colors.GREEN}{Colors.BOLD}🎉 VALIDATION PRODUCTION RÉUSSIE!{Colors.END}")
            print("Le système Arduino est prêt pour la production.")
        elif passed_tests >= total_tests * 0.8:
            print(f"{Colors.YELLOW}{Colors.BOLD}⚠️ VALIDATION PARTIELLEMENT RÉUSSIE{Colors.END}")
            print("Quelques ajustements sont recommandés avant mise en production.")
        else:
            print(f"{Colors.RED}{Colors.BOLD}❌ VALIDATION ÉCHOUÉE{Colors.END}")
            print("Des problèmes critiques doivent être résolus.")
        
        print()
        print("Pour plus d'informations, consultez TROUBLESHOOTING.md")

def main():
    parser = argparse.ArgumentParser(description="Validation de production Arduino mDNS MQTT")
    parser.add_argument('--broker', '-b', help='Adresse IP du broker MQTT')
    parser.add_argument('--topic', '-t', default='/arduino', help='Topic MQTT à surveiller')
    parser.add_argument('--monitor-time', '-m', type=int, default=300, 
                       help='Durée de surveillance MQTT en secondes (default: 300)')
    parser.add_argument('--no-monitor', action='store_true',
                       help='Ignorer la surveillance MQTT')
    
    args = parser.parse_args()
    
    # Gestion des signaux
    def signal_handler(sig, frame):
        print(f"\n{Colors.YELLOW}Validation interrompue par signal{Colors.END}")
        sys.exit(130)
    
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    # Créer et exécuter le validateur
    validator = ProductionValidator(mqtt_broker=args.broker, mqtt_topic=args.topic)
    
    validator.print_header()
    
    # Séquence de tests
    tests = [
        ("Connectivité réseau", validator.test_network_connectivity),
        ("Services mDNS", validator.test_mdns_services),
        ("Connectivité MQTT", validator.test_mqtt_connectivity),
        ("Configuration production", validator.test_production_config)
    ]
    
    # Exécuter les tests de base
    for test_name, test_func in tests:
        print(f"\n{Colors.MAGENTA}🧪 {test_name}...{Colors.END}")
        try:
            test_func()
        except Exception as e:
            print(f"{Colors.RED}❌ Erreur dans {test_name}: {e}{Colors.END}")
            validator.results[test_name.lower().replace(' ', '_')] = False
    
    # Surveillance MQTT si demandée
    if not args.no_monitor:
        print(f"\n{Colors.MAGENTA}🧪 Surveillance MQTT...{Colors.END}")
        try:
            validator.monitor_mqtt_messages(args.monitor_time)
        except Exception as e:
            print(f"{Colors.RED}❌ Erreur surveillance MQTT: {e}{Colors.END}")
    
    # Générer le rapport final
    validator.generate_report()

if __name__ == "__main__":
    main()
