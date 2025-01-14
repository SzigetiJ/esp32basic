#!/bin/sh

## Write binary to ESP32 flash
## Input:
##  <$1> binary file
##  [$2] port (default: /dev/ttyUSB0)
##  [$3] address (default: 0x1000)

ESPTOOL="esptool.py"

if [ $# -lt 1 ]; then
  echo "Usage: $0 <binary> [port [address]]">&2
  exit 1
fi

BINFILE="$1"
USBPORT="/dev/ttyUSB0"
ADDRESS="0x1000"

if [ -n "$2" ]; then
 USBPORT="$2"
fi

if [ -n "$3" ]; then
 ADDRESS="$3"
fi

$ESPTOOL --chip=esp32 --port "$USBPORT" write_flash "$ADDRESS" "$BINFILE" -ff 40m -fm dio
