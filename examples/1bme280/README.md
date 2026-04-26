### Simple application to BME280 sensor

In this small example application we show the usage of BME280.

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
* `I` - Toggle verbose information (initially: off).

#### Practices
