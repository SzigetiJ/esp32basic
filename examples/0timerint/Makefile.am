include $(top_srcdir)/scripts/elf2bin.mk
include $(top_srcdir)/ld/flags.mk

noinst_HEADERS = defines.h

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

AM_CFLAGS  = -std=c11 -flto
AM_CPPFLAGS = -I$(top_srcdir)/src -I$(srcdir)
LDADD = $(top_builddir)/src/libesp32basic.a

bin_PROGRAMS = \
 timer.elf

if WITH_BINARIES
CLEANFILES = \
 timer.bin
endif

BUILT_SOURCES = $(CLEANFILES)
