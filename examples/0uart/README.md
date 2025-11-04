### UART0 TX buffer demo

This example demonstrates how the UART TX buffer can be filled via FIFO register.
We can examine what happens if we try to put more bytes into the buffer than its size allows.
Also, we can modify the UART TX buffer size.

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

#### Control

The application accepts the following commands (on UART0):

* `pNNN` - put NNN bytes into the TX FIFO;
* `bN` - set buffer size to N*128 bytes;
* `i` - print info;
* `h` - print help.
