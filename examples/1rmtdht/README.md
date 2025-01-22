### DHT22 sensor sampling example

In this example we periodically read sensor data (temperature and rel. humidity)
from DHT22 external device.
Special 1-wire protocol is used when communicating to DHT22.
The 1-wire protocol is implemented in `modules/dht22.c`.

#### Hardware components

* S1: DHT22 temp/hum sensor

#### Connections

```
ESP32.GPIO2 -- S1.OUT
ESP32.GND   -- S1.-
ESP32.VCC   -- S1.+
```

#### Practices

