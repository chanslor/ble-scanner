#!/bin/bash
set -e
cd /home/cursor/SECURITY/ble-scanner/server

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Fly CLI path
FLY="$HOME/.fly/bin/fly"
if [ ! -x "$FLY" ]; then
    FLY=$(which fly 2>/dev/null || which flyctl 2>/dev/null || echo "fly")
fi

# Load credentials if available (only export lines, skip commands)
if [ -f "../.env_creds" ]; then
    eval "$(grep '^export ' ../.env_creds)"
fi

# Generate a random API key if not set
if [ -z "$BLE_API_KEY" ]; then
    BLE_API_KEY=$(openssl rand -hex 16)
    echo -e "${YELLOW}Generated new API key: ${BLE_API_KEY}${NC}"
    echo "Add this to your ESP32 secrets.h and save it!"
fi

case "${1:-deploy}" in
    init)
        echo -e "${YELLOW}=== Initializing Fly.io App ===${NC}"

        # Create the app
        $FLY apps create ble-scanner --org personal 2>/dev/null || echo "App may already exist"

        # Create volume for SQLite
        echo -e "${YELLOW}Creating volume for database...${NC}"
        $FLY volumes create ble_data --region dfw --size 1 --yes 2>/dev/null || echo "Volume may already exist"

        # Set secrets
        echo -e "${YELLOW}Setting API key secret...${NC}"
        $FLY secrets set API_KEY="$BLE_API_KEY"

        echo -e "${GREEN}=== Initialization Complete ===${NC}"
        echo ""
        echo "Next steps:"
        echo "  1. Run: ./deploy.sh deploy"
        echo "  2. Update ESP32 secrets.h with:"
        echo "     const char* BLE_SERVER_URL = \"https://ble-scanner.fly.dev/api/logs\";"
        echo "     const char* BLE_API_KEY = \"$BLE_API_KEY\";"
        ;;

    deploy)
        echo -e "${YELLOW}=== Deploying to Fly.io ===${NC}"

        # Deploy
        $FLY deploy

        echo -e "${GREEN}=== Deployment Complete ===${NC}"
        echo ""
        echo "Dashboard: https://ble-scanner.fly.dev/"
        echo "API: https://ble-scanner.fly.dev/api/"
        ;;

    logs)
        echo -e "${YELLOW}=== Streaming Logs ===${NC}"
        $FLY logs
        ;;

    status)
        echo -e "${YELLOW}=== App Status ===${NC}"
        $FLY status
        ;;

    ssh)
        echo -e "${YELLOW}=== SSH into App ===${NC}"
        $FLY ssh console
        ;;

    db)
        echo -e "${YELLOW}=== Database Shell ===${NC}"
        $FLY ssh console -C "sqlite3 /data/ble-scanner.db"
        ;;

    secret)
        if [ -z "$2" ]; then
            echo "Usage: ./deploy.sh secret <API_KEY>"
            exit 1
        fi
        echo -e "${YELLOW}=== Setting API Key ===${NC}"
        $FLY secrets set API_KEY="$2"
        echo -e "${GREEN}API key updated. Update your ESP32 secrets.h!${NC}"
        ;;

    local)
        echo -e "${YELLOW}=== Running Locally ===${NC}"

        # Download dependencies
        go mod tidy

        # Build
        CGO_ENABLED=1 go build -o server .

        # Run
        API_KEY="${BLE_API_KEY:-dev-key}" DB_PATH="./ble-scanner.db" ./server
        ;;

    build)
        echo -e "${YELLOW}=== Building ===${NC}"
        go mod tidy
        CGO_ENABLED=1 go build -o server .
        echo -e "${GREEN}Build complete: ./server${NC}"
        ;;

    *)
        echo "BLE Scanner Server Deploy Script"
        echo ""
        echo "Usage: ./deploy.sh <command>"
        echo ""
        echo "Commands:"
        echo "  init     - Initialize Fly.io app, volume, and secrets"
        echo "  deploy   - Deploy to Fly.io"
        echo "  logs     - Stream application logs"
        echo "  status   - Show app status"
        echo "  ssh      - SSH into the app container"
        echo "  db       - Open SQLite shell on the server"
        echo "  secret   - Set a new API key"
        echo "  local    - Run locally for development"
        echo "  build    - Build the binary locally"
        ;;
esac
