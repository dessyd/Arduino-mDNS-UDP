#!/bin/bash

# ===============================================
# Script de Déploiement Production
# Arduino mDNS MQTT Client - Version Production
# ===============================================

set -euo pipefail

# Couleurs pour output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_NAME="Arduino-mDNS-UDP"
ARDUINO_FQBN="arduino:samd:mkrwifi1010"
DEFAULT_PORT="/dev/ttyACM0"
CONFIG_FILE="config.h"
SECRETS_FILE="arduino_secrets.h"
INO_FILE="Arduino-mDNS-UDP.ino"

# Fonctions utilitaires
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_header() {
    echo -e "${BLUE}"
    echo "=================================================="
    echo "  DÉPLOIEMENT PRODUCTION - $PROJECT_NAME"
    echo "=================================================="
    echo -e "${NC}"
}

check_prerequisites() {
    log_info "Vérification des prérequis..."
    
    # Vérifier arduino-cli
    if ! command -v arduino-cli &> /dev/null; then
        log_error "arduino-cli n'est pas installé"
        log_info "Installation: curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
        exit 1
    fi
    
    # Vérifier les fichiers requis
    local files=("$INO_FILE" "$CONFIG_FILE" "${SECRETS_FILE}.example")
    for file in "${files[@]}"; do
        if [[ ! -f "$file" ]]; then
            log_error "Fichier manquant: $file"
            exit 1
        fi
    done
    
    log_success "Prérequis validés"
}

check_git_branch() {
    log_info "Vérification de la branche Git..."
    
    if [[ -d ".git" ]]; then
        local current_branch
        current_branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
        
        if [[ "$current_branch" != "main" ]]; then
            log_warning "Vous n'êtes pas sur la branche 'main' (actuelle: $current_branch)"
            read -p "Continuer quand même? (y/N): " -n 1 -r
            echo
            if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                log_info "Basculement vers la branche main..."
                git checkout main || {
                    log_error "Impossible de basculer vers main"
                    exit 1
                }
            fi
        fi
        
        log_success "Branche Git validée: $current_branch"
    else
        log_warning "Pas de repository Git détecté"
    fi
}

validate_configuration() {
    log_info "Validation de la configuration production..."
    
    # Vérifier que DEBUG est false
    if grep -q "DEBUG true" "$CONFIG_FILE"; then
        log_error "DEBUG est encore activé dans $CONFIG_FILE"
        log_info "Changement automatique pour production..."
        sed -i.bak 's/DEBUG true/DEBUG false/g' "$CONFIG_FILE"
        log_success "DEBUG désactivé automatiquement"
    fi
    
    # Vérifier les intervalles de production
    local search_interval
    search_interval=$(grep "SEARCH_INTERVAL" "$CONFIG_FILE" | grep -o '[0-9]\+' | head -1)
    
    if [[ "$search_interval" -lt 60000 ]]; then
        log_warning "SEARCH_INTERVAL ($search_interval ms) semble court pour production"
        log_info "Recommandé: >= 60000 ms (1 minute)"
    fi
    
    # Vérifier le fichier secrets
    if [[ ! -f "$SECRETS_FILE" ]]; then
        log_error "Fichier $SECRETS_FILE manquant"
        log_info "Copier depuis l'exemple: cp ${SECRETS_FILE}.example $SECRETS_FILE"
        log_info "Puis éditer avec vos vrais paramètres WiFi"
        exit 1
    fi
    
    # Vérifier que les secrets ne sont pas les exemples
    if grep -q "VotreNomDeReseauWiFi\|VotreMotDePasseWiFi" "$SECRETS_FILE"; then
        log_error "Le fichier $SECRETS_FILE contient encore les valeurs d'exemple"
        log_info "Veuillez configurer vos vrais paramètres WiFi"
        exit 1
    fi
    
    log_success "Configuration production validée"
}

show_configuration_summary() {
    log_info "Résumé de la configuration:"
    echo -e "${YELLOW}"
    echo "  Fichier de configuration: $CONFIG_FILE"
    echo "  DEBUG activé: $(grep 'DEBUG ' "$CONFIG_FILE" | grep -o 'true\|false')"
    echo "  Intervalle recherche mDNS: $(grep 'SEARCH_INTERVAL' "$CONFIG_FILE" | grep -o '[0-9]\+' | head -1) ms"
    echo "  Intervalle publication: $(grep 'PUBLISH_INTERVAL' "$CONFIG_FILE" | grep -o '[0-9]\+' | head -1) ms"
    echo "  Service mDNS: $(grep 'MDNS_SERVICE_TYPE' "$CONFIG_FILE" | grep -o '"[^"]*"' | head -1)"
    echo -e "${NC}"
}

detect_arduino_port() {
    log_info "Détection du port Arduino..."
    
    local detected_ports
    detected_ports=$(arduino-cli board list | grep -E "(Arduino|MKR)" | awk '{print $1}' || true)
    
    if [[ -n "$detected_ports" ]]; then
        local port
        port=$(echo "$detected_ports" | head -1)
        log_success "Arduino détecté sur le port: $port"
        echo "$port"
    else
        log_warning "Aucun Arduino détecté automatiquement"
        echo "$DEFAULT_PORT"
    fi
}

compile_sketch() {
    log_info "Compilation du sketch..."
    
    # Nettoyer d'abord
    arduino-cli cache clean
    
    # Compiler
    if arduino-cli compile --fqbn "$ARDUINO_FQBN" "$INO_FILE" --verbose; then
        log_success "Compilation réussie"
        return 0
    else
        log_error "Erreur de compilation"
        return 1
    fi
}

upload_sketch() {
    local port="$1"
    log_info "Upload vers Arduino sur $port..."
    
    # Vérifier que le port existe
    if [[ ! -e "$port" ]]; then
        log_error "Port $port inexistant"
        return 1
    fi
    
    # Upload avec vérification
    if arduino-cli upload -p "$port" --fqbn "$ARDUINO_FQBN" "$INO_FILE" --verify; then
        log_success "Upload réussi avec vérification"
        return 0
    else
        log_error "Erreur d'upload"
        return 1
    fi
}

test_mqtt_connectivity() {
    log_info "Test de connectivité MQTT..."
    
    # Rechercher un broker MQTT sur le réseau
    log_info "Recherche de services mDNS MQTT..."
    if command -v avahi-browse &> /dev/null; then
        local mqtt_services
        mqtt_services=$(timeout 10 avahi-browse -t _mqtt._tcp --resolve --parsable | head -5)
        
        if [[ -n "$mqtt_services" ]]; then
            log_success "Services MQTT détectés:"
            echo "$mqtt_services" | while IFS=';' read -r interface protocol name type domain hostname address port txt; do
                if [[ "$protocol" == "IPv4" && -n "$address" && -n "$port" ]]; then
                    echo "  - $name à $address:$port"
                fi
            done
        else
            log_warning "Aucun service MQTT trouvé via mDNS"
        fi
    else
        log_warning "avahi-browse non disponible, test mDNS ignoré"
    fi
    
    # Test basique de connectivité réseau
    log_info "Test de connectivité réseau..."
    if ping -c 1 -W 3 8.8.8.8 >/dev/null 2>&1; then
        log_success "Connectivité Internet OK"
    else
        log_warning "Pas de connectivité Internet détectée"
    fi
}

monitor_arduino() {
    local port="$1"
    local duration="${2:-30}"
    
    log_info "Monitoring Arduino pendant $duration secondes..."
    log_info "Appuyez sur Ctrl+C pour arrêter plus tôt"
    
    if command -v arduino-cli &> /dev/null; then
        timeout "$duration" arduino-cli monitor -p "$port" -c baudrate=9600 2>/dev/null || true
    else
        log_warning "arduino-cli monitor non disponible"
        log_info "Utilisez: screen $port 9600 ou minicom"
    fi
}

create_backup() {
    log_info "Création d'une sauvegarde..."
    
    local timestamp
    timestamp=$(date +%Y%m%d_%H%M%S)
    local backup_dir="backup_${timestamp}"
    
    mkdir -p "$backup_dir"
    
    # Sauvegarder les fichiers importants
    cp "$CONFIG_FILE" "$backup_dir/"
    cp "$SECRETS_FILE" "$backup_dir/" 2>/dev/null || log_warning "Pas de secrets à sauvegarder"
    cp "$INO_FILE" "$backup_dir/"
    
    # Créer un fichier de métadonnées
    cat > "$backup_dir/deploy_info.txt" << EOF
Sauvegarde automatique - Déploiement Production
Date: $(date)
Branch: $(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
Commit: $(git rev-parse HEAD 2>/dev/null || echo "unknown")
Config: DEBUG=$(grep 'DEBUG ' "$CONFIG_FILE" | grep -o 'true\|false')
EOF
    
    log_success "Sauvegarde créée dans: $backup_dir"
}

perform_health_check() {
    local port="$1"
    log_info "Vérification de santé post-déploiement..."
    
    log_info "Attente de démarrage de l'Arduino (15 secondes)..."
    sleep 15
    
    # En mode production, pas de sortie série de debug
    # On ne peut vérifier que la présence du device
    if [[ -e "$port" ]]; then
        log_success "Arduino présent sur $port"
    else
        log_error "Arduino non détecté sur $port"
        return 1
    fi
    
    log_info "En mode production, vérifiez manuellement:"
    echo "  1. Messages MQTT reçus: mosquitto_sub -h broker -t '/arduino'"
    echo "  2. Consommation électrique < 50mA"
    echo "  3. Pas de reconnexions WiFi fréquentes"
}

show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -p, --port PORT     Port série Arduino (détection auto si omis)"
    echo "  -h, --help          Afficher cette aide"
    echo "  --no-upload         Compiler seulement (pas d'upload)"
    echo "  --no-monitor        Ne pas monitorer après upload"
    echo "  --no-backup         Ne pas créer de sauvegarde"
    echo ""
    echo "Exemple:"
    echo "  $0                  # Déploiement automatique complet"
    echo "  $0 -p /dev/ttyUSB0  # Spécifier le port manually"
    echo "  $0 --no-upload      # Compilation seulement"
}

main() {
    local port=""
    local do_upload=true
    local do_monitor=true
    local do_backup=true
    
    # Parsing des arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -p|--port)
                port="$2"
                shift 2
                ;;
            -h|--help)
                show_usage
                exit 0
                ;;
            --no-upload)
                do_upload=false
                shift
                ;;
            --no-monitor)
                do_monitor=false
                shift
                ;;
            --no-backup)
                do_backup=false
                shift
                ;;
            *)
                log_error "Option inconnue: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    print_header
    
    # Étapes de déploiement
    check_prerequisites
    check_git_branch
    validate_configuration
    show_configuration_summary
    
    # Demander confirmation
    echo ""
    read -p "Continuer le déploiement production? (y/N): " -n 1 -r
    echo ""
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_info "Déploiement annulé par l'utilisateur"
        exit 0
    fi
    
    # Sauvegarde
    if [[ "$do_backup" == true ]]; then
        create_backup
    fi
    
    # Compilation
    if ! compile_sketch; then
        log_error "Échec de compilation - arrêt du déploiement"
        exit 1
    fi
    
    # Upload si demandé
    if [[ "$do_upload" == true ]]; then
        # Détecter le port si non spécifié
        if [[ -z "$port" ]]; then
            port=$(detect_arduino_port)
        fi
        
        log_info "Port utilisé: $port"
        
        if ! upload_sketch "$port"; then
            log_error "Échec d'upload - vérifiez la connexion Arduino"
            exit 1
        fi
        
        # Tests post-déploiement
        test_mqtt_connectivity
        perform_health_check "$port"
        
        # Monitoring si demandé
        if [[ "$do_monitor" == true ]]; then
            echo ""
            read -p "Lancer le monitoring série? (y/N): " -n 1 -r
            echo ""
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                monitor_arduino "$port" 60
            fi
        fi
    fi
    
    # Succès final
    echo ""
    log_success "🎉 DÉPLOIEMENT PRODUCTION TERMINÉ AVEC SUCCÈS!"
    echo ""
    echo -e "${GREEN}Étapes suivantes:${NC}"
    echo "  1. Vérifier réception messages MQTT"
    echo "  2. Surveiller la consommation électrique"
    echo "  3. Configurer monitoring long terme"
    echo "  4. Documenter l'installation"
    echo ""
    echo -e "${BLUE}Commandes utiles:${NC}"
    echo "  mosquitto_sub -h votre-broker -t '/arduino' -v"
    echo "  arduino-cli monitor -p $port -c baudrate=9600"
    echo ""
}

# Gestion des signaux
trap 'log_warning "Déploiement interrompu par signal"; exit 130' INT TERM

# Point d'entrée
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi