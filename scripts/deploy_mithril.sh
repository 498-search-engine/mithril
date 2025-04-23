#!/bin/bash
# Mithril deployment script

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' 

echo -e "${GREEN}Deploying Mithril Search Engine...${NC}"

# 1. Enable and start the systemd service
echo "Starting Mithril service..."
sudo systemctl daemon-reload
sudo systemctl enable mithril
sudo systemctl restart mithril
sleep 2

# Check if service is running
if sudo systemctl is-active --quiet mithril; then
    echo -e "${GREEN}✓ Mithril service started successfully${NC}"
else
    echo -e "${RED}✗ Failed to start Mithril service${NC}"
    echo "Check logs with: sudo journalctl -u mithril -n 50"
    exit 1
fi

# 2. Ensure Nginx is running
echo "Configuring Nginx..."
sudo systemctl enable nginx
sudo systemctl restart nginx

if sudo systemctl is-active --quiet nginx; then
    echo -e "${GREEN}✓ Nginx started successfully${NC}"
else
    echo -e "${RED}✗ Failed to start Nginx${NC}"
    echo "Check logs with: sudo journalctl -u nginx -n 50"
    exit 1
fi

echo -e "${GREEN}Deployment complete!${NC}"
echo "Mithril search engine should now be available at mithril.mdvsh.co / <PRIVATE_IP>"
echo "Monitor logs with: sudo journalctl -f -u mithril"
