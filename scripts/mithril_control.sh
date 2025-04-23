#!/bin/bash
# Mithril control script

case "$1" in
  start)
    echo "Starting Mithril service..."
    sudo systemctl start mithril
    ;;
  stop)
    echo "Stopping Mithril service..."
    sudo systemctl stop mithril
    ;;
  restart)
    echo "Restarting Mithril service..."
    sudo systemctl restart mithril
    ;;
  status)
    echo "Mithril service status:"
    sudo systemctl status mithril
    ;;
  logs)
    echo "Showing Mithril logs:"
    sudo journalctl -u mithril -n "${2:-50}"
    ;;
  nginx-logs)
    echo "Showing Nginx logs:"
    sudo tail -n "${2:-50}" /var/log/nginx/error.log
    ;;
  *)
    echo "Usage: $0 {start|stop|restart|status|logs [lines]|nginx-logs [lines]}"
    exit 1
    ;;
esac

exit 0
