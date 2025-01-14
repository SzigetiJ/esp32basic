### Listening on button press event

This example demonstrates how to set up an ISR on GPIO event.
First, input direction must be enabled on the GPIO pin,
and direct peripherial input must be configured.
Next, the interrupt must be set up (GPIO PIN register).
Finally, ISR has to be assigned to the interrupt (`_xtos_set_interrupt_handler_arg`).

In DPORT, the interrupt channel is assigned to every GPIO interrupts.
Thus, within the ISR (`button_isr`), it must be checked where, on which GPIO has the interrupt occured.
After the interrupt is served, within the ISR, the interrupt status has to be cleared.

#### Hardware components

* B0: BOOT button

#### Connections

```
ESP32.GPIO0 -- B0 (builtin) -- ESP32.GND
```
