AM_CFLAGS  = -nostdlib -std=c11 -flto

libesp32basic_a_AR=$(AR) rcs

lib_LIBRARIES = libesp32basic.a

include_HEADERS = dport.h esp32types.h esp_attr.h gpio.h i2c.h \
 iomux.h lockmgr.h main.h pidctrl.h print.h rmt.h romfunctions.h rtc.h timg.h \
 typeaux.h uart.h xtutils.h \
 utils/i2cutils.h utils/i2ciface.h utils/rmtutils.h utils/uartutils.h utils/generators.h
nodist_include_HEADERS =

libesp32basic_a_SOURCES = i2c.c lockmgr.c main.c timg.c utils/i2cutils.c utils/rmtutils.c utils/uartutils.c utils/generators.c
nodist_libesp32basic_a_SOURCES =

CLEANFILES =

BUILT_SOURCES = $(CLEANFILES)
