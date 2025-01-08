Note, all the files `esp32.rom*.ld` are taken from ESP-IDF without any modification.

Memory layout and sections are defined in `esp32.ld`.
It is a very simple solution, no ROM sections are defined / used at all.

`esp32peripheral.ld` defines register base addresses for global peripherals
defined as extern variables in `src/*.h`.
