### Making beep music with RMT

RMT supports signal modulation with a carrier wave (square waveform).
In this example we modify the wavelength of the carrier to produce tones on
different frequencies.

The program plays first a small music piece (sequence of notes),
the some variations of the piece.
The music-related constants, types and functions can be found in [music.h](music.h) and [music.c](music.c).

Seems like the RMT peripheral uses some kind of internal buffering,
and denotes RMT RAM registers as **done** / **free** before they were processed and output signal was sent to GPIO / IOMUX.
Hence, the `TXTHRES` interrupt comes sooner than `TXLIM` entry pairs were sent out.
The current workaround is to increase the initial `TXLIM` value by `2`.

Most of the notes consist of active and inactive parts
(except for the pause, which is has only inactive part,
and the continuous not, which has only active part),
and the active part always preceeds the inactive part.
The original idea was to produce `TXTHRES` interrupt right after the active part
of the note has been sent out.
This way we might have avoided that pitch change happens when the next note already started.
Unfortunately, currently this feature does not work.
Still, you can find traces of this idea in the code (e.g., `u8RmtRamLastLoLen`, `u8RmtRamCurHiLen` and `u8RmtRamCurLoLen`).

#### Hardware components

* SP: 8 Ω speaker
* R1: 200 Ω -- 470 Ω resistor

#### Connections

```
ESP32.GPIO2 -- R1 -- SP -- ESP32.GND
```

#### Practices

1. Write your own music (piece and variations).

2. Make some UART control, e.g., start / stop playback; list / select music.
