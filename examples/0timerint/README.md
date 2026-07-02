### Example on CCOUNT / CCOMPARE interrupt usage

In this example we show how to use `TIMER.n` (`CCOUNT` / `CCOMPAREn`) interrupts.
We use the timer interrupt to take precise periodic measuements.
The provided ISR reads current time from default timer and stores the value.
Also, after a given number of invocations the ISR detaches itself.

#### Control

* `0`, `1`,`2` selects `ccompare0`, `ccompare1` or `ccompare2` for measurement (default: 0).
* `[`,`]` for decrease/increase sampling length by `1`.
* `{`,`}` for decrease/increase sampling length by `10`.
* `,`,`.` for decrease/increase interrupt period by `1`.
* `<`,`>` for decrease/increase interrupt period by `10`.
* `s` starts the measurement.
* `r` prints the result of the last measurement.
* `C` prints some register and pointer information for debugging.
* `g` use general ISR (with xthal calls, slower, default).
* `G` use timer-specific ISR (with `SRS()`/`WSR()` calls).

#### Experience

* `TIMER.0` general interrupt period gets stable (no jitter) around 264 ccompare increment (and above), (i.e., 1.65 µs, running at 160MHz).
* `TIMER.1` general interrupt period gets stable around 230 ccompare increment (and above), (i.e., 1.44 µs, running at 160MHz).
* `TIMER.2` general interrupt period gets stable around 484 ccompare increment (and above), (i.e., 3 µs, running at 160MHz).
Note, however, `xt_highint5` is not implemented yet, i.e., we use the general ISR written in C.

Using the timer-specific ISRs with hardwired `RSR()`/`WSR()` macros,
the interrupt period gets stable at 220 (`TIMER.0`) and 178 (`TIMER.1`) ccompare increment (meaning 1.4 µs, 1.1 µs).

Note, the highest interrupt frequency we get with `TIMER.0` or `TIMER.1` is almost twice as high as the best frequency we get
using [TIMG ALARM interrupt](../0timgalarm)).
