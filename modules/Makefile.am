AM_CPPFLAGS = -I$(top_srcdir)/src
AM_CFLAGS  = -nostdlib -mlongcalls -std=c11 -flto
AM_LDFLAGS = -Wl,--gc-sections -T $(top_srcdir)/esp32.ld

libesp32modules_a_AR=$(AR) rcs

lib_LIBRARIES = libesp32modules.a

include_HEADERS = bme280.h
nodist_include_HEADERS =

libesp32modules_a_SOURCES = bme280.c
nodist_libesp32modules_a_SOURCES =

CLEANFILES =

BUILT_SOURCES = $(CLEANFILES)
