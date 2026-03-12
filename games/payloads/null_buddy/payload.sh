#!/bin/sh
# NULL-BUDDY — Cyber-Tamagotchi for WiFi Pineapple Pager
# D-pad: Navigate  |  A: Confirm  |  Hold Red: Quit

GAME="/root/games/null-buddy/null_buddy"

if [ ! -x "$GAME" ]; then
    echo "ERROR: $GAME not found. Deploy games first."
    exit 1
fi

exec "$GAME"
