## ESP32Basic scripts

### flash.sh

This script calls `esptool.py write_flash` with prepared parameters.
As first argument, it requires the filename of the binary subject to write to the flash memory of ESP32.
Additionally, optionally, it also accepts second and third parameters:
the usb port where the esp32 device is attached (default: `/dev/ttyUSB0`) and
the memory (start) address to write the binary to (default: `0x1000`).

Note, `0x1000` is the address where the 1st stage bootloader assumes the 2nd stage bootloader is located.
Thus, using the default script options you write the application in place of the 2nd stage bootloader.
And there you have some limitations regarding the application.

* During the 2nd stage bootloading, the first `1024` bytes of the IRAM (starting at 0x40080000)
are reserved for interrupt/exception vectors.
And the code can begin at 0x40080400.
This circumstance is inherited by the application when written in place of the 2nd stage bootloader.

* The 1st stage bootloader does not activate cache mechanism.
Thus, your application has to activate cache for itself.

* Also, there might be limitation for the size of the application binary, IRAM / DRAM / IROM / DROM access,
but I have not checked these limitations yet.

Anyway, it is also possible to use the 2nd stage bootloader and partition table provided by ESP-IDF,
and write the application binary to the (factory) application partition.
However, the `bootloader` application provided by `esp-idf` (v5.2) expects
that the binary of the application to load has IROM and DROM sections defined.
If there are no such sections defined in the binary, the `bootloader`,
more specifically, `cache_ll_l1_get_bus()` aborts and resets.

Other benefits of using the 2nd stage bootloader:
AFAIK, the flash / flash controller within the ESP32 does not provide any wear leveling to extend the lifetime of the flash.
Without 2nd stage bootloader, the blocks starting at 0x1000 get overwritten everytime you flash your application onto the device.
If those blocks get worn, your ESP32 device becomes practically useless, since the 1st stage bootloader reads always from 0x1000.
However, if you keep writing your application to 0x10000 and those blocks get worn, you can still choose another offset
for the application (e.g., 0x110000, 0x210000, etc.) and modify the partition table only once, not even overwriting the bootloader.

#### Workaround

To be able to use the esp-idf bootloader with esp32basic application, I had to slightly modify the bootloader
so that in case `drom_size` or `irom_size` equals 0, it can skip the corresponding `cache_ll_l1_get_bus()`
(and consequently `cache_ll_l1_enable_bus()`) calls.

#### Example

First, define a partition table as csv file:
```
# Name,   Type, SubType, Offset,  Size, Flags
nvs,data,nvs,0x9000,0x5000,
factory,app,factory,0x10000,0x3F0000,
```

Convert it into binary format:
```
gen_esp32part.py partitions.csv > partitions.bin
```

Write it to offset `0x8000` (default partition table location):
```
esptool.py write_flash 0x8000 partitions.bin
```

Get somehow a bootloader binary, e.g., from the `build/bootloader` directory of your existing esp-idf application.
Note, check the configuration of the bootloader:

* flash size is 4MB,
* partition table offset is 0x8000,
* no RTC reset should be called.

Write the bootloader to the flash:
```
esptool.py write_flash 0x1000 bootloader.bin
```

If anything goes wrong, see the uart output of the bootloader / app (monitor.sh).
Maybe you need more information, so increase the verbosity level of the bootloader (idf menuconfig).
Often, this step results in a larger bootloader binary, so that is will not fit into the 0x7000 area,
and you have to shift the offset of the partition table
(and the offset of the nvs partition within the partition table).

Finally, you can write your application to offset 0x10000:
```
./flash.sh my_app.bin /dev/ttyUSB0 0x10000
```

### monitor.sh

This script is a wrapper to `serial.tools.miniterm` python script.
See its headcomment on *how to use* information.