include $(top_srcdir)/scripts/elf2bin.mk

noinst_HEADERS = defines.h

AM_CFLAGS  = -nostdlib -mlongcalls -std=c11 -flto
AM_LDFLAGS = -Wl,--gc-sections -T $(top_srcdir)/esp32.ld
AM_CPPFLAGS = -I$(top_srcdir)/src -I$(srcdir)
LDADD = $(top_builddir)/src/libesp32basic.a

bin_PROGRAMS = \
 rmtmorse.elf

rmtmorse_elf_SOURCES = mphgen.c rmtmorse.c

CLEANFILES = \
 rmtmorse.bin

BUILT_SOURCES = $(CLEANFILES)
