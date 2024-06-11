### Periodic LED blinking

This example shows how to directly set/clear a GPIO output.
We call `gpio_reg_setbit()` with parameter either `&gsGPIO.OUT_W1TC` (clear) or `&gsGPIO.OUT_W1TS` (set)
instead of calling `gpio_pin_enable()` or `gpio_pin_disable()`.

#### Hardware components

* D1: ...
* R1: 470 Ω resistor

#### Connections

```
ESP32.GPIO2 -- R1 --(A) D1 (K)-- ESP32.GND
```
