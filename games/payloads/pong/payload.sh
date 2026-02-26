#!/bin/sh
# Pong — Classic pong game for WiFi Pineapple Pager
# D-pad Up/Down: Move paddle  |  Hold Red: Quit

GAME="/root/games/pong/pong"

if [ ! -x "$GAME" ]; then
    echo "ERROR: $GAME not found. Deploy games first."
    exit 1
fi

exec "$GAME"
