#!/bin/sh
# NETRUNNER 2084 — Cyberpunk turn-based RPG for WiFi Pineapple Pager
# D-pad: Move  |  A: Confirm  |  B: Inventory  |  Hold Red: Quit

GAME="/root/games/netrunner/netrunner"

if [ ! -x "$GAME" ]; then
    echo "ERROR: $GAME not found. Deploy games first."
    exit 1
fi

exec "$GAME"
