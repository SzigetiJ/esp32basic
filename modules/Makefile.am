AM_CPPFLAGS = -I$(top_srcdir)/src
AM_CFLAGS  = -nostdlib -mlongcalls -std=c11 -flto

libesp32modules_a_AR=$(AR) rcs

lib_LIBRARIES = libesp32modules.a

include_HEADERS = bh1750.h bme280.h
nodist_include_HEADERS =

libesp32modules_a_SOURCES = bh1750.c bme280.c
nodist_libesp32modules_a_SOURCES =

CLEANFILES =

BUILT_SOURCES = $(CLEANFILES)
