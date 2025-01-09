## Bilis ESP32 Basic

![GitHub](https://img.shields.io/github/license/SzigetiJ/esp32basic)
![C/C++ CI](https://github.com/SzigetiJ/esp32basic/workflows/C/C++%20CI/badge.svg)
[![GitHub code size](https://img.shields.io/github/languages/code-size/SzigetiJ/esp32basic)](https://github.com/SzigetiJ/esp32basic)
![GitHub repo size](https://img.shields.io/github/repo-size/SzigetiJ/esp32basic)
![GitHub commit activity](https://img.shields.io/github/commit-activity/y/SzigetiJ/esp32basic)
![GitHub issues](https://img.shields.io/github/issues/SzigetiJ/esp32basic)
![GitHub closed issues](https://img.shields.io/github/issues-closed/SzigetiJ/esp32basic)

_A small application framework for ESP32_

Some words about this project.

Originally, I did my ESP32 development using the SDK provided by ESP-IDF.
But soon it turned out that it does not fit to my purposes. Why?

 * The way the source code in ESP-IDF is organized leads to problems during the application development process.
Here I mean the header dependencies. (And header dependencies also imply component dependencies).
Just try to create a very simple "hello, world!" (UART) or led blinking (GPIO (& Timer)) project
and see all the unselected / unwanted components (e.g., network protocol stack) getting compiled and linked
taking away your precious time and space.

 * FreeRTOS is a good thing, I like it, but most of the time I do not use the features it provides.

 * Digging deeper into the ESP-IDF SDK code, you realize that the peripheral access methods are synchronous (i.e., blocking).
I you want to create non-blocking peripheral access you have to write your own low-level code. And that is what I am doing.

 * Finally, take a look at the blink example of the `get-started` codes in ESP-IDF.
I cannot understand, how and why driving LED strip became a 'get-started' level of ESP32 coding.

### Quick start guide

* Install required tools (see Requirements below).

* Set your environment variables (`$PATH` for `xtensa-esp-elf` and `$IDF_TOOLS_PATH`(?) for `esptool.py`)

* Follow [installation instruction](INSTALL) until `make` (`make install` is not reuqired).
Personally, I always do multiple `VPATH` builds with different configuration settings.
In order to use the `xtensa-esp-elf` toolchain, you have to call the configure script with option
`--host=xtensa-esp32-elf`.

* Hopefully, the build runs without any error, and
the `examples` subdirectory (within the build / dist / VPATH directory) contains binaries (`.bin` files).

* Prepare your ESP32 device (there must be a figure in the README of the given example showing pin connections).

* Flash the binary to the device. Use either your own flashing tool or [my flashing script](scripts/flash.sh).

#### Writing your own Bilis ESP32 application

_TODO_

The user application must define the variables and functions declared in [main.h](src/main.h):

```c
  extern const bool gbStartAppCpu;
  extern const uint16_t gu16Tim00Divisor;
  extern const uint64_t gu64tckSchedulePeriod;

  void prog_init_pro_pre();
  void prog_init_app();
  void prog_init_pro_post();

  void prog_cycle_pro(uint64_t u64tckCur);
  void prog_cycle_app(uint64_t u64tckCur);
```

Of course, if `gbStartAppCpu` is set to `false`, you can leave `void prog_init_app()` and `void prog_cycle_app(uint64_t u64tckCur)` empty.
`prog_init_pro_pre()` gets called before starting the APP CPU, whereas `prog_init_pro_post()` is called _after_ APP CPU is launched.

With `gu16Tim00Divisor` you can set the granularity of `TG0_Timer0` (I usually set it to `2` resulting in 40MHz timer frequency).
Finally, `gu64tckSchedulePeriod` determines how often (i.e., when) `prog_cycle_pro()` and `prog_cycle_app()` functions will be called by
the scheduler.


### Features

THis list is supposed to grow longer and longer.

* Asynchronous (non-blocking) peripheral access.
* Mutexes, locks (lock manager) allowing shared resource usage (e.g., I2C bus).
* Low level peripheral access (via registers).
* Peripheral controller drivers
  * I2C
  * RMT (TODO: example on RX)
  * _TODO_ SPI
  * _TODO_ etc.
* Device drivers
  * BME280 Temperature / Pressure / Humidity sensor
  * (_TODO_: cleanup) BH1750 Light sensor
* etc (_TODO_)

### Simplifications

* No FreeRTOS.
* No HAL.
* Most of the peripheral drivers are not implemented yet.
* I know it is a big gap, but currently interrupts are not masked in during critical system routines.
* Memory layout and sections defined in the [linker file](ld/esp32.ld) are very simple.

### Requirements

 * [Autotools](https://www.gnu.org/software/automake/), since Bilis ESP32 Basic is an automake project
(suggested versions: automake:1.16.1, autoconf:2.69).

 * [xtensa-esp-elf](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/idf-tools.html) toolchain (compiler, linker, etc.)
(suggested versions: 13.2.0).

 * [esptool](https://github.com/espressif/esptool) Currently, the flash tools (see scripts) rely on `esptool.py`
(suggested versions: v4.7.0).

## Contribution

I really appreciate if you find this project interesting, all the bug reports and feature requests are welcome!
But I do not know how much time and effort I can spend on the development.
Feel free to fork this project and play around with it integrating your own sensor or other device driver into it.

## About license

The memory arrangement (see esp32.ld) and start-up code (main.c) is based on [esp32-minimal](https://github.com/aykevl/esp32-minimal).
This project also contains code (see xtutils.h) directly taken from the official [ESP32 SDK](https://github.com/espressif/esp-idf),
which is released under Apache-2.0.
