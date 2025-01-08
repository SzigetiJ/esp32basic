AM_LDFLAGS = -Wl,--gc-sections -specs=nano.specs \
 -T $(top_srcdir)/ld/esp32.ld \
 -T $(top_srcdir)/ld/esp32peripheral.ld \
 -T $(top_srcdir)/ld/esp32.rom.newlib-funcs.ld 

