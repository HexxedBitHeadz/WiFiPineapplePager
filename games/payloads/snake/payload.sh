#!/bin/sh
# Snake — Classic snake game for WiFi Pineapple Pager
# D-pad: Move  |  Hold Red: Quit

GAME="/root/games/snake/snake"

if [ ! -x "$GAME" ]; then
    echo "ERROR: $GAME not found. Deploy games first."
    exit 1
fi

exec "$GAME"
