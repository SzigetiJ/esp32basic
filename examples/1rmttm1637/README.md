### TM1637 display example

This example demonstrates how to use the 3 different flush methods of the `TM1637` module.
In this example, the state of the display has an outer cycle and an inner subcycle.
The outer cycle defines what characters to display.
Here, these are hexadecimal numbers form `0` to `F`.
(Each digit displays the same number.)
The inner subcycle is 6 steps long and is called **phase** (see `u8Phase`):

1. First phase displays the numbers with separator colon/dot after `TM1637_COLON_POS`.
2. Next, the separator colon/dot is removed.
3. The brightness level is lowered.
4. The brightness level is raised.
5. The separator is displayed again, and the caracters after the separator are displaying `-`.
6. The separator is removed.

#### Hardware components

* X1: TM1637 display

#### Connections

```
ESP32.GPIO21 -- X1.CLK
ESP32.GPIO19 -- X1.DIO
ESP32.GND    -- X1.GND
ESP32.VCC    -- X1.VCC
```

#### Practices

