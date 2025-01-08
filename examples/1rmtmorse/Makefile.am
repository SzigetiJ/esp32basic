include $(top_srcdir)/scripts/elf2bin.mk
include $(top_srcdir)/ld/flags.mk
AM_LDFLAGS += -T $(top_srcdir)/ld/esp32.rom.ld

noinst_HEADERS = defines.h mphgen.h

AM_CFLAGS  = -std=c11 -flto
AM_CPPFLAGS = -I$(top_srcdir)/src -I$(srcdir)
LDADD = $(top_builddir)/src/libesp32basic.a

bin_PROGRAMS = \
 rmtmorse.elf

rmtmorse_elf_SOURCES = mphgen.c rmtmorse.c

if WITH_BINARIES
CLEANFILES = \
 rmtmorse.bin
endif

BUILT_SOURCES = $(CLEANFILES)
