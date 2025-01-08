include $(top_srcdir)/scripts/elf2bin.mk
include $(top_srcdir)/ld/flags.mk

noinst_HEADERS = defines.h

AM_CFLAGS  = -std=c11 -flto
AM_CPPFLAGS = -I$(top_srcdir)/src -I$(srcdir)
LDADD = $(top_builddir)/src/libesp32basic.a

bin_PROGRAMS = \
 hello.elf

if WITH_BINARIES
CLEANFILES = \
 hello.bin
endif

BUILT_SOURCES = $(CLEANFILES)
