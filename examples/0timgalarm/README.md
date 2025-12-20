### Measuring TIMG alarm accuracy

This example examines periodic TIMG alarm behaviour for very low alarm values.

#### Control

* `1`,`2`, `3` selects `timer0_1`,`timer1_0` or `timer1_1` for measurement.
* `a`,`b` selects how to implement recurrency: `a` for timer autoreload, `b` for incrementing `ALARM` value of the timer.
* `[`,`]` for decrease/increase sampling length by `1`.
* `{`,`}` for decrease/increase sampling length by `10`.
* `,`,`.` for decrease/increase alarm period by `1`.
* `<`,`>` for decrease/increase alarm period by `10`.
* `s` starts the measurement.
* `r` prints the result of the last measurement.
* `c` prints some register information for debugging.
