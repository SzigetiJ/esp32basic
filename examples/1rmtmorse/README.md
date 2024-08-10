### Blinking a Morse-encoded message with RMT

With Morse codes we can encode text messages to digital carrier.
The Morse code of the character is a sequence of long and short ON pulses
separated by relatively short OFF gaps.
As the length of the symbols is not fixed (varying between 1 and 5),
we use a longer gap between symbols (letters).

In this example we encode the text `"Hello, World!"` and use the RMT controller
to send the encoded message to a GPIO output pin.

In this example the filling procedure of the RMT TX buffer is quite complex.

* First, we need to transform the character of the text message to Morse codes.

* Then, the Morse symbols must be transformed into a sequence on/off signals.

* The sequence of on/off signals requires timings (ms).

* This sequence must be transformed into RMT entries, where the time resolution is µs,
and the.entries are shorter than `2^15`.

* Obviously, such a long sequence cannot fit into a single RMT RAM block, moreover,
all the 8 RAM blocks still are not sufficient, thus, based on tx threshold alerts
(setting the `TX_LIM` register and periodically scanning the `TX_THRES_EVENT` interrupt),
we feed the RMT RAM in `TX_LIM` sized segments.

The source input is the text message.
The aforementioned sequences are derived directly or indirectly from it.
However, these derives sequences could be long taking up much memory.
Moreover, the size of these sequences is dynamic, and we cannot calculate those sizes in advance.
(Actually, we could calculate them, but again, it would be complex).
So we do not create the derived sequences in whole in advance.
Instead, we use *generators*, more precisely, a *chain of generators*.

* At the end of the chain, we have to fill the RMT RAM with **pairs of entries**.
So there is a *pair generator* (see `_pairgen_next()` in [rmtutils.c](../../src/utils/rmtutils.c)),
which takes 2 RMT entries as input.

* The RMT entries have their period in µs resolution.
This entries are derived from input entries of ms resolution using the *stretch generator*
(also [rmtutils.c](../../src/utils/rmtutils.c)).

* The *MorsePhase To Entry* generator is quite simple, as it only makes a 1:1 transformation from Morse phase to RMT entry [rmtmorse.c](rmtmorse.c).

* But the *MorsePhase To Entry* generator relies on the *MorsePhase Generator*,
 which produces sequence of Morse-phases out of a text character [mphgen.c](mphgen.c).

* The text charactes are provided by the *Character generator*,
and this generator is the beginning of the chain [chargen.c](chargen.c).
It takes a text message as parameter to iterate on.

#### Hardware components

* D1: ...
* R1: 470 Ω resistor

#### Connections

```
ESP32.GPIO2 -- R1 --(A) D1 (K)-- ESP32.GND
```

#### Practices

1. Play around with the memory size allocated to the RMT channel.

2. The ESP32 reference manual states that in `TX_CONTI_MODE`
"there will be an idle level lasting one clk_div cycle between N and N+1 transmissions."
Solve this problem somehow.

3. Instead of periodic register scanning, use an ISR.

4. Instead of static text message, take the source text message from UART0 console input.
(transition start should be triggered by newline character).

5. Replace D1 with a speaker and make audio output.
To achieve this you have to modulate the RMT pulses with a _carrier_ wave.
The setup of the carrier is already implemented, you just have to switch on (set `CARRIER_EN` to `1`).
What you still can do is playing around with the carrier frequency / duty cycle.
