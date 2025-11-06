### UART TX transfer performance comparison

In this example we compare UART FIFO feed speed to UDMA transfer.
The application performs repeatedly the following phases:

* 1st phase - do UART FIFO feed;
* 2nd phase - print UART FIFO feed duration;
* 3rd phase - initiate UDMA TX transfer;
* 4th phase - print UDMA transfer duration.

Between phases 3 and 4 we catch UHCI TX complete interrupt.
After the last phase the length of the transferred message is increased (from 100 bytes up to 1000 bytes).

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
