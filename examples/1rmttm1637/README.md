### TM1637 display example

In this example the TM1637 displays hexadecimal numbers form `0` to `F`.
Each digit displays the same number.
The colon between the 2nd and the 3rd digits is blinking every second.
Each number is displayed for 3 seconds:
1 second full brightness, 1 second medium brightness and 1 second low brightness.


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

