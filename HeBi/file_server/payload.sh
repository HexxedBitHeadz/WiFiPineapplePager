#!/bin/bash
# Title:       HeBi File Server
# Author:      HeBi
# Description: HTTP file server with upload and download support.
#              Stdlib only - no pip, no dependencies.
#              Supports foreground (with kill switch) or background mode.
# Version:     1.3

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_PORT=8080
DEFAULT_DIR="/root"
DEFAULT_USER="hebi"
PID_FILE="/tmp/hebi_httpd.pid"

# ----------------------------------------------------------------
# Check if already running — offer to stop it
# ----------------------------------------------------------------
if [ -f "$PID_FILE" ]; then
    EXISTING_PID=$(cat "$PID_FILE")
    if kill -0 "$EXISTING_PID" 2>/dev/null; then
        resp=$(CONFIRMATION_DIALOG "File server already running (PID $EXISTING_PID). Stop it?")
        if [ "$resp" = "1" ]; then
            kill "$EXISTING_PID" 2>/dev/null
            rm -f "$PID_FILE"
            LOG green "File server stopped."
        fi
        exit 0
    else
        rm -f "$PID_FILE"
    fi
fi

# ----------------------------------------------------------------
# Pick port
# ----------------------------------------------------------------
PORT=$(NUMBER_PICKER "HTTP server port" "$DEFAULT_PORT")
case $? in
    $DUCKYSCRIPT_CANCELLED) exit 0 ;;
    $DUCKYSCRIPT_REJECTED)  PORT=$DEFAULT_PORT ;;
    $DUCKYSCRIPT_ERROR)     PORT=$DEFAULT_PORT ;;
esac

# ----------------------------------------------------------------
# Pick directory
# ----------------------------------------------------------------
SERVE_DIR=$(TEXT_PICKER "Directory to serve" "$DEFAULT_DIR")
case $? in
    $DUCKYSCRIPT_CANCELLED) exit 0 ;;
    $DUCKYSCRIPT_REJECTED)  SERVE_DIR=$DEFAULT_DIR ;;
    $DUCKYSCRIPT_ERROR)     SERVE_DIR=$DEFAULT_DIR ;;
esac

# ----------------------------------------------------------------
# Pick password (leave blank to disable auth)
# ----------------------------------------------------------------
PASSWORD=$(TEXT_PICKER "Password (blank = no auth)" "")
case $? in
    $DUCKYSCRIPT_CANCELLED) exit 0 ;;
    $DUCKYSCRIPT_REJECTED)  PASSWORD="" ;;
    $DUCKYSCRIPT_ERROR)     PASSWORD="" ;;
esac

# ----------------------------------------------------------------
# Foreground or background?
# CONFIRMATION_DIALOG outputs "1" for confirm, "0" for cancel
# ----------------------------------------------------------------
RUN_BG=$(CONFIRMATION_DIALOG "Run in background? YES=bg (relaunch to stop) NO=fg (kill switch)")

# ----------------------------------------------------------------
# Ensure directory exists
# ----------------------------------------------------------------
if [ ! -d "$SERVE_DIR" ]; then
    mkdir -p "$SERVE_DIR" 2>/dev/null
    if [ ! -d "$SERVE_DIR" ]; then
        LOG red "Directory not found: $SERVE_DIR"
        ERROR_DIALOG "Directory not found:\n$SERVE_DIR"
        exit 1
    fi
fi

# ----------------------------------------------------------------
# Get IP for display
# ----------------------------------------------------------------
IP=$(ip -4 addr show br-lan 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+' | head -1)
[ -z "$IP" ] && IP=$(hostname -I 2>/dev/null | awk '{print $1}')
[ -z "$IP" ] && IP="pager-ip"

# ----------------------------------------------------------------
# Start server
# ----------------------------------------------------------------
LOG blue "Starting file server..."
LOG blue "Serving: $SERVE_DIR"
if [ -n "$PASSWORD" ]; then
    LOG blue "Auth: enabled (user: $DEFAULT_USER)"
else
    LOG yellow "Auth: disabled"
fi
LOG blue "URL: http://$IP:$PORT"

python3 "$SCRIPT_DIR/httpd.py" "$PORT" "$SERVE_DIR" "$DEFAULT_USER" "$PASSWORD" &
SERVER_PID=$!

sleep 1

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    LOG red "Server failed to start - port $PORT may be in use"
    ERROR_DIALOG "Server failed to start.\nPort $PORT may be in use.\n\nTry a different port."
    exit 1
fi

echo "$SERVER_PID" > "$PID_FILE"
LOG green "File server running (PID $SERVER_PID)"

if [ -n "$PASSWORD" ]; then
    ALERT "File Server Running" "http://$IP:$PORT\n\nUser: $DEFAULT_USER\nPass: $PASSWORD\n\nServing: $SERVE_DIR"
else
    ALERT "File Server Running" "http://$IP:$PORT\n\nNo auth\n\nServing: $SERVE_DIR"
fi

# ----------------------------------------------------------------
# Foreground: block with kill switch, then clean up
# Background: exit payload, server keeps running
# ----------------------------------------------------------------
if [ "$RUN_BG" = "1" ]; then
    # Background mode — payload exits, server keeps running
    LOG green "Running in background."
    LOG green "Launch this payload again to stop."
else
    # Foreground mode — wait for user to stop it
    CONFIRMATION_DIALOG "File server running at http://$IP:$PORT — click YES to stop."
    kill "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null
    rm -f "$PID_FILE"
    LOG green "File server stopped."
fi
