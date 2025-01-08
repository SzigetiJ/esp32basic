include $(top_srcdir)/scripts/elf2bin.mk
include $(top_srcdir)/ld/flags.mk
AM_LDFLAGS += -T $(top_srcdir)/ld/esp32.rom.ld \
 -T $(top_srcdir)/ld/esp32.rom.redefined.ld

noinst_HEADERS = defines.h music.h

AM_CFLAGS  = -nostdlib -mlongcalls -std=c11 -flto
AM_CPPFLAGS = -I$(top_srcdir)/src -I$(srcdir)
LDADD = $(top_builddir)/src/libesp32basic.a

bin_PROGRAMS = \
 rmtmusic.elf

rmtmusic_elf_SOURCES = music.c rmtmusic.c

CLEANFILES = \
 rmtmusic.bin

BUILT_SOURCES = $(CLEANFILES)
