### LED blinking with RMT

In this example GPIO on/off timings are controlled by RMT.
D1 makes fast (0.1s long) blinking (with 0.1s gaps).
The main cycle starts the RMT TX process every 2 seconds.
First, a single blink is issued, next 2 blinks, etc. until 8 blinks.
Then the number of blinks starts again with 1.

#### Hardware components

* D1: ...
* R1: 470 Ω resistor

#### Connections

```
ESP32.GPIO2 -- R1 --(A) D1 (K)-- ESP32.GND
```
