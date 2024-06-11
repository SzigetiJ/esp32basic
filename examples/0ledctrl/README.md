### LED controlling with UART input

In this example, we use UART input to control LED state (on/off).
The following control keys are recognized:

* `y` switch on the yellow LED
* `Y` switch off the yellow LED
* `r` switch on the red LED
* `R` switch off the red LED

#### Hardware components

* D1: (yellow LED)
* D2: (red LED)
* R1: 470 Ω resistor

#### Connections

```
ESP32.GPIO2 -- (A) D1 (K) -- NODE0
ESP32.GPIO4 -- (A) D2 (K) -- NODE0
NODE0 -- R1 -- ESP32.GND
```
