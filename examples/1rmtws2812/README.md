### Driving WS2812 LED strip

In this example different color gradients are smoothly rotating around in a 12 LED long strip.

#### Hardware components

* LS1: 12 LED long 3-wire WS2812B strip

#### Connections

```
ESP32.GND    -- LS1.GND
ESP32.GPIO21 -- LS1.DIN
EDP32.3V3    -- LS1.+5V
```

#### Practices

1. Play around with strip length, front length, etc. constants.
2. Play around with iteration constants and update times.
3. Make a 3 LED long segment run forth and back on the strip.
4. Make 12 independent blinking LEDs.
5. Make 6 independent blinking LEDs and on the other 6 LEDs do gradient cycling.
6. Do gradient rotation with a single gradient, but allow to override gradient colors from terminal (UART) given in hex format.
