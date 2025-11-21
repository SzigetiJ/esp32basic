### Driving SSD1306 OLED display

This simple example demonstates the effect of different SSD1306 commands.
You can control the application via UART0 terminal.

#### Hardware components

OLED1 -- 32x128 or 64x128 SSD1306 OLED display

#### Connections

```
ESP32.GND    -- OLED1.GND
ESP32.GPIO22 -- OLED1.SCL
ESP32.GPIO23 -- OLED1.SDA
ESP32.3V3    -- OLED1.VCC
```

#### Control

The application accepts the following commands (on UART0):

* `+` / `-` - Brightness up / down;
* `w` / `s` - set display offset up / down;
* `i` / `k` - set display start line up / down;
* `9` / `0` - decrease / increase multiplex ratio;
* `r` - reverse display (change COM scan direction and segment assignment);
* `c` - change font size `8x8` -> `8x6` -> `6x4` and reset printing the whole character set;
* `x` - invert display;
* `e` - entire display on / off.

#### Practices
