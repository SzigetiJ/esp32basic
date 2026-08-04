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

* `p` (OK) select `PLL_CLK` as clock source;
* `x` (OK) select `XTL_CLK` as clock source;
* `]` (OK) set higher PLL-based clock frequency;
* `[` (OK) set lower PLL-based clock frequency;
* `-` (OK) set higher XTL_CLK divisor (lower clock frequency);
* `+` (OK) set lower XTL_CLK divisor (higher clock frequency);
* `<` (OK) set lower dig vreg dbias;
* `>` (OK) set higher dig vreg dbias;
* `u` (OK) set lower UART0 baud;
* `U` (OK) set higher UART0 baud;
* `a` (OK) select UART0 clock source (`APB_CLK` or `REF_TICK`);
* `i` (OK) Information about registers;
* `I` (OK) display current settings;
* `r` (OK) when re-entering a CPU freq state, use its last known vreg dbias setting;
* `R` (OK) always use the default vreg dbias setting for any CPU freq state.
