include $(top_srcdir)/scripts/elf2bin.mk
include $(top_srcdir)/ld/flags.mk

noinst_HEADERS = defines.h

AM_CFLAGS  = -std=c11 -flto
if WITH_BINARIES
AM_LDFLAGS += \
 -T $(top_srcdir)/ld/esp32.rom.ld \
 -T $(top_srcdir)/ld/esp32.rom.libgcc.ld \
 -T $(top_srcdir)/ld/esp32.rom.newlib-data.ld \
 -T $(top_srcdir)/ld/esp32.rom.newlib-locale.ld \
 -T $(top_srcdir)/ld/esp32.rom.newlib-nano.ld \
 -T $(top_srcdir)/ld/esp32.rom.newlib-time.ld \
 -T $(top_srcdir)/ld/esp32.rom.redefined.ld \
 -T $(top_srcdir)/ld/esp32.rom.syscalls.ld
else
AM_LDFLAGS += \
 -T $(top_srcdir)/ld/esp32.rom.ld \
 -T $(top_srcdir)/ld/esp32.rom.libgcc.ld \
 -T $(top_srcdir)/ld/esp32.rom.redefined.ld \
 -T $(top_srcdir)/ld/esp32.rom.syscalls.ld
endif

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(srcdir)
LDADD = $(top_builddir)/src/libesp32basic.a

bin_PROGRAMS = \
 rmtws2812.elf

rmtws2812_elf_SOURCES = rmtws2812.c

if WITH_BINARIES
CLEANFILES = \
 rmtws2812.bin
endif

BUILT_SOURCES = $(CLEANFILES)
