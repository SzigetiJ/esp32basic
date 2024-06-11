### Hello World on UART0

This example demonstrates how to flush messages to UART channel.
`UART0` is automatically configured by the underlying low-level OS,
we just have to set the communication speed of it.

#### Hardware components

You have to access the `UART0` channel of the ESP32 somehow.
Probably, you have flashed the binary to ESP32 over USB line
and at the ESP32 end there is an *UART_over_USB* module (e.g., `cp210x converter`).
So if you have an USB cable connecting the ESP32 module to your PC, that is fine.

#### Software requirements

On your PC, choose your favorite serial terminal tool.
My choice is *miniterm*:
```sh
python -m serial.tools.miniterm --filter=direct /dev/ttyUSB0 115200 --rts 0
```
