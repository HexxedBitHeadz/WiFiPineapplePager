#!/bin/sh
# Packet Catcher — Catch falling packets on WiFi Pineapple Pager
# D-pad Left/Right: Move  |  Hold Red: Quit

GAME="/root/games/packet_catcher/packet_catcher"

if [ ! -x "$GAME" ]; then
    echo "ERROR: $GAME not found. Deploy games first."
    exit 1
fi

exec "$GAME"
