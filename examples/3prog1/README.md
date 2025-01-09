### An example of higher complexity level (sandbox)

Here we have different I2C slave devices connected to the same I2C channel.
Sensors are periodically scanned, and measurements are written to UART console.
LEDs are blinking alternately, blinking period is set by TIMG alarm interrupt.
A simple pattern is being written to the OLED display from left to right.

#### Hardware components

* D1: (yellow LED)
* D2: (red LED)
* R1: 470 Ω resistor
* I2C_SLAVE#1: BME280 temperature/pressure/humidity sensor
* I2C_SLAVE#2: BH1750FVI light sensor
* I2C_SLAVE#3: 32x128 OLED display
* [NODE0 .. NODE4]: 5 nodes (maybe on breadboard) as connection nodes of multiple wires.

#### Connections

```
ESP32.GPIO2 -- (A) D1 (K) -- NODE0
ESP32.GPIO4 -- (A) D2 (K) -- NODE0
NODE0 -- R1 -- ESP32.GND

ESP32.GPIO22 -- NODE1 (I2C.SCL)
ESP32.GPIO23 -- NODE2 (I2C.SDA)
ESP32.3V3 -- NODE3 (I2C.VCC)
ESP32.GND -- NODE4 (I2C.GND)

BME280.VCC -- NODE3
BME280.GND -- NODE4
BME280.SCL -- NODE1
BME280.SDA -- NODE2
BME280.CSB -- NODE3
BME280.SDO -- NODE4

BH1750.VCC -- NODE3
BH1750.GND -- NODE4
BH1750.SCL -- NODE1
BH1750.SDA -- NODE2
BH1750.ADDR -- NODE4

OLED.GND -- NODE4
OLED.VCC -- NODE3
OLED.SCL -- NODE1
OLED.SDA -- NODE2
```
