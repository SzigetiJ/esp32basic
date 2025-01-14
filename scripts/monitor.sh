#!/bin/sh

## Creates serial uart connection to the given device
## Input:
##  [$1] port (default: /dev/ttyUSB0)
##  [$2] speed (default: 115200)
PORT=/dev/ttyUSB0
SPEED=115200

if [ -n "$1" ]; then
 PORT="$1"
fi

if [ -n "$2" ]; then
 SPEED="$2"
fi

python -m serial.tools.miniterm --filter=direct "$PORT" "$SPEED"
