### Simple application to BME280 sensor

In this small example application we show how to use the BME280 sensor.

#### Hardware components

BME280 -- BME280 sensor

#### Connections

```
ESP32.GND    -- BME280.GND
ESP32.GPIO22 -- BME280.SCL
ESP32.GPIO23 -- BME280.SDA
ESP32.3V3    -- BME280.VCC
ESP32.GPIO21 -- BME280.CSB
ESP32.GND    -- BME280.SDO
```

#### Control

The application accepts the following commands (on UART0):

* `-` / `+` - T Oversampling down (off) / up;
* `[` / `]` - P Oversampling down (off) / up;
* `{` / `}` - H Oversampling down (off) / up;
* `<` / `>` - t_standby up / down;
* `,` / `.` - measurement period up / down in forced mode;
* `f` / `n` / `s` - TODO: set mode: forced / normal / sleep;
* `c` - read config/control/status registers to local mirror;
* `r` - reset;
* `i` - information;
* `I` - Toggle verbose information (initially: off);
* `3` / `4` - Decrease / Increase communication speed (SCL frequency, default: 100KHz, multipliers: ..., 0.5, 1, 2, 5, 10, 20, ...);
* `#` / `$` - Decrease / Increase communication speed (smooth multipliers: 1.0, 1.1, 1.2, 1.3, ..., 1.9(, 2.0, 2.1, ..., 2.4));
* `;` / `'` - Decrease / Increase IIR filter value.

Note: The maximal I2C SCL frequency on esp32basic is 5.5 MHz,
this is the upper bound of communication speed the application allows.
However, the BME280 Specificaton states that `Digital interface  I2C (up to 3.4 MHz)` (page 2).
With the current I2C timing setting (SCL high/low period, START/STOP/SDA delays),
the maximal communication speed is 1.8 MHz.

#### Practices
