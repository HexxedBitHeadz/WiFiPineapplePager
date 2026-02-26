#!/bin/sh
# Test Hardware — Visual button + display test for WiFi Pineapple Pager
# Shows D-pad state on screen  |  Hold A+B to quit

TOOL="/root/tests/test_hw"

if [ ! -x "$TOOL" ]; then
    echo "ERROR: $TOOL not found. Deploy tests first."
    exit 1
fi

exec "$TOOL"
