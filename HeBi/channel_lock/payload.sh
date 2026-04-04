#!/bin/bash
# Title:       Channel Lock
# Author:      Hexxed BitHeadz
# Description: Prompts the user to select a WiFi channel and lock duration,
#              then locks the Pineapple to that channel for the specified time.
# Version:     1.0

# Step 1: Pick WiFi channel (1-13; common: 1, 6, 11)
channel=$(NUMBER_PICKER "WiFi Channel (1-13)" 6)
case $? in
    $DUCKYSCRIPT_CANCELLED)
        LOG "User cancelled channel selection."
        exit 0
        ;;
    $DUCKYSCRIPT_REJECTED | $DUCKYSCRIPT_ERROR)
        LOG "Channel selection failed."
        exit 1
        ;;
esac

# Step 2: Pick duration in minutes (5, 10, 15, 30, 60)
minutes=$(NUMBER_PICKER "Duration in minutes" 10)
case $? in
    $DUCKYSCRIPT_CANCELLED)
        LOG "User cancelled duration selection."
        exit 0
        ;;
    $DUCKYSCRIPT_REJECTED | $DUCKYSCRIPT_ERROR)
        LOG "Duration selection failed."
        exit 1
        ;;
esac

# Convert minutes to seconds — strip any whitespace NUMBER_PICKER may leave
channel=$(printf '%s' "$channel" | tr -d '[:space:]')
minutes=$(printf '%s' "$minutes" | tr -d '[:space:]')
seconds=$(( minutes * 60 ))

# Validate before calling the macro
if ! [[ "$channel" =~ ^[0-9]+$ ]] || [ "$channel" -lt 1 ] || [ "$channel" -gt 13 ]; then
    LOG "ERROR: invalid channel '$channel' — must be 1-13"
    exit 1
fi
if [ "$seconds" -lt 1 ]; then
    LOG "ERROR: invalid duration '$minutes' min ($seconds sec)"
    exit 1
fi

# Step 3: Lock channel — PINEAPPLE_EXAMINE_CHANNEL also pauses the hopper
PINEAPPLE_EXAMINE_CHANNEL "$channel" "$seconds"

# Step 4: Confirm on screen
ALERT "Channel $channel locked for $minutes min ($seconds sec)"

# Step 5: Vibrate to confirm lock is active
VIBRATE "alert"
