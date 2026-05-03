
#!/bin/bash
# Title:       Evilginx Portal Stop
# Author:      Hexxed BitHeadz
# Description: Tears down the evilginx portal — restores dnsmasq, kills nc server,
#              removes iptables rules.
# Version:     1.0

DNSMASQ_CONF="/etc/dnsmasq.conf"
BACKUP_FILE="/tmp/evilginxportal_dnsmasq.bak"
NC_PID_FILE="/tmp/evilginxportal_nc.pid"
PAGER_IP="172.16.52.1"

resp=$(CONFIRMATION_DIALOG "Stop Evilginx Portal?\n\nThis removes iptables rules, kills redirect server, and restores dnsmasq.")
[ "$resp" != "1" ] && exit 0

# Kill nc redirect server
if [ -f "$NC_PID_FILE" ]; then
    LOG blue "Stopping redirect server (PID $(cat $NC_PID_FILE))..."
    kill "$(cat $NC_PID_FILE)" 2>/dev/null
    pkill -f "nc -l -p 8080" 2>/dev/null
    rm -f "$NC_PID_FILE"
    LOG green "Redirect server stopped"
else
    LOG yellow "No PID file found — attempting pkill..."
    pkill -f "nc -l -p 8080" 2>/dev/null
fi

# Remove iptables rules
LOG blue "Removing iptables rules..."
iptables -t nat -D PREROUTING -i br-lan -p tcp --dport 80 \
    -j DNAT --to-destination "$PAGER_IP:8080" 2>/dev/null
iptables -D FORWARD -p tcp --dport 853 -j DROP 2>/dev/null
iptables -D FORWARD -p udp --dport 853 -j DROP 2>/dev/null
LOG green "iptables rules removed"

# Restore dnsmasq
if [ -f "$BACKUP_FILE" ]; then
    LOG blue "Restoring dnsmasq.conf from backup..."
    cp "$BACKUP_FILE" "$DNSMASQ_CONF"
    rm -f "$BACKUP_FILE"
else
    LOG yellow "No backup found — stripping injected lines manually..."
    sed -i '/^# --- DoH Kill ---/,/^$/d' "$DNSMASQ_CONF"
    sed -i '/^# --- Captive Portal/,/^$/d' "$DNSMASQ_CONF"
    sed -i '/^address=\//d' "$DNSMASQ_CONF"
fi

LOG blue "Restarting dnsmasq..."
/etc/init.d/dnsmasq restart 2>&1 | while IFS= read -r line; do LOG blue "$line"; done
sleep 1

LOG green "Evilginx Portal torn down. DNS restored to clean state."
ALERT "Evilginx Portal Stopped" "iptables cleared\nRedirect server killed\ndnsmasq restored"
VIBRATE "alert"