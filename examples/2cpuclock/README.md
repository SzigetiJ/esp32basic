### Example setting CPU clock source and related things

CPU clock can have the following sources:

* `XTL_CLK`
* `PLL_CLK`
* `RC_FAST_CLK`
* `APLL_CLK`

Currently we support `XTL_CLK` and `PLL_CLK` with the following frequencies:

* `XTL_CLK`
  * 40 MHz
  * 20 MHz
  * 10 MHz
  * 8 MHz
  * 5 MHz
* `PLL_CLK`
  * 80 MHz
  * **160 MHz** (default)
  * 240 MHz

In this example you can select the cpu clock source,
set the clock frequency,
tune the digital voltage regulator (dig_dbias) and
select the UART0 baud rate (115200, 57600, or 9600).

The NodeMCU onboard LED (GPIO2) blinks at 1 Hz rate based on APB clock.
This feature is useful to see wether the APB clock frequency matches the
expected value.

#### Control

* `p` select `PLL_CLK` as clock source;
* `x` select `XTL_CLK` as clock source;
* `]` set higher PLL-based clock frequency;
* `[` set lower PLL-based clock frequency;
* `-` set higher XTL_CLK divisor (lower clock frequency);
* `+` set lower XTL_CLK divisor (higher clock frequency);
* `<` set lower dig vreg dbias;
* `>` set higher dig vreg dbias;
* `u` set lower UART0 baud;
* `U` set higher UART0 baud;
* `a` select UART0 clock source (`APB_CLK` or `REF_TICK`);
* `i` Information about registers;
* `I` display current settings;
* `r` when re-entering a CPU freq state, use its last known vreg dbias setting;
* `R` always use the default vreg dbias setting for any CPU freq state.
