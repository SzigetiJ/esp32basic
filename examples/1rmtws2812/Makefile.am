include $(top_srcdir)/scripts/elf2bin.mk

noinst_HEADERS = defines.h

AM_CFLAGS  = -nostdlib -mlongcalls -std=c11 -flto
AM_LDFLAGS = -Wl,--gc-sections -T $(top_srcdir)/esp32.ld
AM_CPPFLAGS = -I$(top_srcdir)/src -I$(srcdir)
LDADD = $(top_builddir)/src/libesp32basic.a

bin_PROGRAMS = \
 rmtws2812.elf

rmtws2812_elf_SOURCES = rmtws2812.c

CLEANFILES = \
 rmtws2812.bin

BUILT_SOURCES = $(CLEANFILES)
