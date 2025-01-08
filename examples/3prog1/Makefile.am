include $(top_srcdir)/scripts/elf2bin.mk
include $(top_srcdir)/ld/flags.mk
AM_LDFLAGS += -T $(top_srcdir)/ld/esp32.rom.ld \
 -T $(top_srcdir)/ld/esp32.rom.redefined.ld \
 -T $(top_srcdir)/ld/esp32.rom.libgcc.ld

noinst_HEADERS = defines.h

AM_CFLAGS  = -std=c11 -flto
AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/modules
LDADD = $(top_builddir)/src/libesp32basic.a $(top_builddir)/modules/libesp32modules.a

bin_PROGRAMS = \
 prog.elf

if WITH_BINARIES
CLEANFILES = \
 prog.bin
endif

BUILT_SOURCES = $(CLEANFILES)
