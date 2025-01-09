include $(top_srcdir)/scripts/elf2bin.mk
include $(top_srcdir)/ld/flags.mk

noinst_HEADERS = defines.h

AM_CFLAGS  = -std=c11 -flto
AM_CPPFLAGS = -I$(top_srcdir)/src -I$(srcdir)
AM_LDFLAGS += -T $(top_srcdir)/ld/esp32.rom.ld \
 -T $(top_srcdir)/ld/esp32.rom.redefined.ld
LDADD = $(top_builddir)/src/libesp32basic.a

bin_PROGRAMS = \
 buttonisr.elf

if WITH_BINARIES
CLEANFILES = \
 buttonisr.bin
endif

BUILT_SOURCES = $(CLEANFILES)
