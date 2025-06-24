# Makefile pour Arduino mDNS MQTT Client - Production
# Facilite les opérations courantes de déploiement et maintenance

# Configuration
FQBN = arduino:samd:mkrwifi1010
PORT = /dev/ttyACM0
PROJECT = Arduino-mDNS-UDP
SKETCH = $(PROJECT).ino
CONFIG_FILE = config.h
SECRETS_FILE = arduino_secrets.h

# Couleurs pour affichage
GREEN = \033[0;32m
YELLOW = \033[1;33m
RED = \033[0;31m
NC = \033[0m # No Color

.PHONY: help setup compile upload deploy test clean validate monitor production debug

# Affichage de l'aide par défaut
help:
	@echo "$(GREEN)Arduino mDNS MQTT Client - Production$(NC)"
	@echo "========================================"
	@echo ""
	@echo "Commandes disponibles:"
	@echo ""
	@echo "  $(YELLOW)setup$(NC)      - Configuration initiale (secrets, dépendances)"
	@echo "  $(YELLOW)compile$(NC)    - Compilation du sketch"
	@echo "  $(YELLOW)upload$(NC)     - Upload vers Arduino (compile + upload)"
	@echo "  $(YELLOW)deploy$(NC)     - Déploiement production complet"
	@echo "  $(YELLOW)test$(NC)       - Tests et validation"
	@echo "  $(YELLOW)monitor$(NC)    - Monitoring série"
	@echo "  $(YELLOW)validate$(NC)   - Validation production complète"
	@echo "  $(YELLOW)clean$(NC)      - Nettoyage fichiers temporaires"
	@echo ""
	@echo "  $(YELLOW)production$(NC) - Basculer en mode production"
	@echo "  $(YELLOW)debug$(NC)      - Basculer en mode debug"
	@echo ""
	@echo "Exemples:"
	@echo "  make setup                    # Configuration initiale"
	@echo "  make deploy PORT=/dev/ttyUSB0 # Déploiement sur port spécifique"
	@echo "  make validate                 # Validation complète"

# Configuration initiale
setup:
	@echo "$(GREEN)🔧 Configuration initiale...$(NC)"
	@if [ ! -f "$(SECRETS_FILE)" ]; then \
		echo "Création du fichier de secrets..."; \
		cp $(SECRETS_FILE).example $(SECRETS_FILE); \
		echo "$(YELLOW)⚠️ Éditez $(SECRETS_FILE) avec vos paramètres WiFi$(NC)"; \
	else \
		echo "$(GREEN)✅ Fichier de secrets existant$(NC)"; \
	fi
	@echo "Vérification des dépendances..."
	@command -v arduino-cli >/dev/null 2>&1 || { \
		echo "$(RED)❌ arduino-cli non installé$(NC)"; \
		echo "Installation: curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"; \
		exit 1; \
	}
	@echo "$(GREEN)✅ Configuration terminée$(NC)"

# Vérification de la configuration
check-config:
	@echo "$(GREEN)🔍 Vérification configuration...$(NC)"
	@if [ ! -f "$(SECRETS_FILE)" ]; then \
		echo "$(RED)❌ Fichier $(SECRETS_FILE) manquant$(NC)"; \
		echo "Lancez: make setup"; \
		exit 1; \
	fi
	@if grep -q "VotreNom" $(SECRETS_FILE); then \
		echo "$(RED)❌ Fichier $(SECRETS_FILE) contient des exemples$(NC)"; \
		echo "Éditez le fichier avec vos vrais paramètres"; \
		exit 1; \
	fi
	@echo "$(GREEN)✅ Configuration validée$(NC)"

# Compilation
compile: check-config
	@echo "$(GREEN)🔨 Compilation...$(NC)"
	arduino-cli compile --fqbn $(FQBN) $(SKETCH)
	@echo "$(GREEN)✅ Compilation terminée$(NC)"

# Upload
upload: compile
	@echo "$(GREEN)📤 Upload vers Arduino...$(NC)"
	@if [ ! -e "$(PORT)" ]; then \
		echo "$(YELLOW)⚠️ Port $(PORT) non trouvé, détection automatique...$(NC)"; \
		PORT_AUTO=$$(arduino-cli board list | grep -E "(Arduino|MKR)" | awk '{print $$1}' | head -1); \
		if [ -n "$$PORT_AUTO" ]; then \
			echo "$(GREEN)✅ Arduino détecté sur $$PORT_AUTO$(NC)"; \
			arduino-cli upload -p $$PORT_AUTO --fqbn $(FQBN) $(SKETCH) --verify; \
		else \
			echo "$(RED)❌ Aucun Arduino détecté$(NC)"; \
			exit 1; \
		fi \
	else \
		arduino-cli upload -p $(PORT) --fqbn $(FQBN) $(SKETCH) --verify; \
	fi
	@echo "$(GREEN)✅ Upload terminé$(NC)"

# Déploiement production complet
deploy:
	@echo "$(GREEN)🚀 Déploiement production...$(NC)"
	@chmod +x deploy_production.sh
	./deploy_production.sh
	@echo "$(GREEN)✅ Déploiement terminé$(NC)"

# Tests rapides
test:
	@echo "$(GREEN)🧪 Tests rapides...$(NC)"
	@echo "Test de connectivité réseau..."
	@ping -c 1 8.8.8.8 >/dev/null 2>&1 && echo "$(GREEN)✅ Internet OK$(NC)" || echo "$(YELLOW)⚠️ Pas d'Internet$(NC)"
	@echo "Test mDNS..."
	@command -v avahi-browse >/dev/null 2>&1 && { \
		timeout 5 avahi-browse -t _mqtt._tcp --resolve --parsable 2>/dev/null | head -1 | grep -q "=" && \
		echo "$(GREEN)✅ Services MQTT détectés$(NC)" || echo "$(YELLOW)⚠️ Aucun service MQTT$(NC)"; \
	} || echo "$(YELLOW)⚠️ avahi-browse non disponible$(NC)"

# Validation production complète
validate:
	@echo "$(GREEN)🔍 Validation production...$(NC)"
	@if [ -f "production_validation.py" ]; then \
		python3 production_validation.py --monitor-time 60; \
	else \
		echo "$(YELLOW)⚠️ Script de validation non trouvé$(NC)"; \
		make test; \
	fi

# Monitoring série
monitor:
	@echo "$(GREEN)📡 Monitoring série (Ctrl+C pour arrêter)...$(NC)"
	@PORT_USED=$(PORT); \
	if [ ! -e "$$PORT_USED" ]; then \
		PORT_USED=$$(arduino-cli board list | grep -E "(Arduino|MKR)" | awk '{print $$1}' | head -1); \
	fi; \
	if [ -n "$$PORT_USED" ]; then \
		echo "Monitoring sur $$PORT_USED..."; \
		arduino-cli monitor -p $$PORT_USED -c baudrate=9600; \
	else \
		echo "$(RED)❌ Aucun port Arduino détecté$(NC)"; \
	fi

# Nettoyage
clean:
	@echo "$(GREEN)🧹 Nettoyage...$(NC)"
	@rm -f *.backup
	@rm -rf backup_*
	@rm -f arduino_debug.log
	@rm -f *.tmp
	@echo "$(GREEN)✅ Nettoyage terminé$(NC)"

# Basculer en mode production
production:
	@echo "$(GREEN)🏭 Basculement mode production...$(NC)"
	@git checkout main 2>/dev/null || echo "$(YELLOW)⚠️ Pas de repository git$(NC)"
	@if [ -f "config-production.h" ]; then \
		cp config-production.h config.h; \
		echo "$(GREEN)✅ Configuration production activée$(NC)"; \
	else \
		echo "$(YELLOW)⚠️ Fichier config-production.h non trouvé$(NC)"; \
	fi
	@grep "DEBUG false" config.h && echo "$(GREEN)✅ DEBUG désactivé$(NC)" || echo "$(RED)❌ DEBUG encore actif$(NC)"

# Basculer en mode debug
debug:
	@echo "$(GREEN)🐛 Basculement mode debug...$(NC)"
	@git checkout Debug 2>/dev/null || echo "$(YELLOW)⚠️ Pas de branche Debug$(NC)"
	@if grep -q "DEBUG false" config.h; then \
		sed -i.backup 's/DEBUG false/DEBUG true/g' config.h; \
		echo "$(GREEN)✅ DEBUG activé$(NC)"; \
	else \
		echo "$(GREEN)✅ DEBUG déjà actif$(NC)"; \
	fi

# Informations système
info:
	@echo "$(GREEN)📋 Informations système$(NC)"
	@echo "=================="
	@echo "Projet: $(PROJECT)"
	@echo "Port: $(PORT)"
	@echo "FQBN: $(FQBN)"
	@echo "Arduino CLI: $$(arduino-cli version 2>/dev/null || echo 'Non installé')"
	@echo "Branche Git: $$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo 'Inconnue')"
	@echo "Configuration DEBUG: $$(grep 'DEBUG ' config.h | grep -o 'true\|false' || echo 'Non défini')"
	@echo "Dernière compilation: $$(stat -c %y build/ 2>/dev/null | cut -d. -f1 || echo 'Jamais')"

# Installation des dépendances
install-deps:
	@echo "$(GREEN)📦 Installation des dépendances...$(NC)"
	@echo "Installation arduino-cli..."
	@curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
	@echo "Configuration arduino-cli..."
	@arduino-cli config init
	@arduino-cli core update-index
	@arduino-cli core install arduino:samd
	@echo "$(GREEN)✅ Dépendances installées$(NC)"

# Affichage des logs récents
logs:
	@echo "$(GREEN)📄 Logs récents$(NC)"
	@if [ -f "arduino_production.log" ]; then \
		echo "=== Logs production (10 dernières lignes) ==="; \
		tail -10 arduino_production.log; \
	else \
		echo "$(YELLOW)⚠️ Pas de logs production$(NC)"; \
	fi

# Surveillance MQTT en arrière-plan
mqtt-monitor:
	@echo "$(GREEN)📡 Démarrage surveillance MQTT...$(NC)"
	@if command -v mosquitto_sub >/dev/null 2>&1; then \
		nohup mosquitto_sub -h localhost -t "/arduino" -v >> arduino_production.log 2>&1 & \
		echo $$! > mqtt_monitor.pid; \
		echo "$(GREEN)✅ Surveillance démarrée (PID: $$(cat mqtt_monitor.pid))$(NC)"; \
		echo "Arrêt: make mqtt-stop"; \
	else \
		echo "$(RED)❌ mosquitto_sub non disponible$(NC)"; \
	fi

# Arrêt surveillance MQTT
mqtt-stop:
	@if [ -f "mqtt_monitor.pid" ]; then \
		kill $$(cat mqtt_monitor.pid) 2>/dev/null || true; \
		rm -f mqtt_monitor.pid; \
		echo "$(GREEN)✅ Surveillance MQTT arrêtée$(NC)"; \
	else \
		echo "$(YELLOW)⚠️ Aucune surveillance active$(NC)"; \
	fi