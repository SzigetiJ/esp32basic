AC_PREREQ([2.69])
AC_INIT([esp32basic],[1.0.0])
AM_INIT_AUTOMAKE
: ${CFLAGS="-g -Os -Wall -nostdlib -mlongcalls"} ${LDFLAGS="-specs=nano.specs"}
AC_PROG_CC
AC_PROG_CC_STDC
AC_DEFUN([AC_PROG_AR], [AC_CHECK_TOOL(AR, ar, :)])
AC_PROG_AR
AC_PROG_RANLIB

AC_ARG_WITH([binaries], AS_HELP_STRING([--without-binaries], [Do not generate .bin images (esptool.py required).]))
AM_CONDITIONAL([WITH_BINARIES], [test x$with_binaries != xno])
AM_COND_IF([WITH_BINARIES],[ AC_MSG_NOTICE([generate .bin images]) ])

AC_CONFIG_SUBDIRS([src])
AC_CONFIG_SUBDIRS([modules])
AC_CONFIG_SUBDIRS([examples])
AC_CONFIG_SUBDIRS([examples/0blink])
AC_CONFIG_SUBDIRS([examples/0hello])
AC_CONFIG_SUBDIRS([examples/0ledctrl])
AC_CONFIG_SUBDIRS([examples/1rmtblink])
AC_CONFIG_SUBDIRS([examples/1rmtmorse])
AC_CONFIG_SUBDIRS([examples/1rmtmusic])
AC_CONFIG_SUBDIRS([examples/1rmtws2812])
AC_CONFIG_SUBDIRS([examples/3prog1])
AC_CONFIG_SUBDIRS([ld])

AC_CONFIG_FILES([
  Makefile
  src/Makefile
  modules/Makefile
  examples/Makefile
  examples/0blink/Makefile
  examples/0button/Makefile
  examples/0hello/Makefile
  examples/0ledctrl/Makefile
  examples/1rmtblink/Makefile
  examples/1rmtmorse/Makefile
  examples/1rmtmusic/Makefile
  examples/1rmtws2812/Makefile
  examples/3prog1/Makefile
  ld/Makefile
])

AC_OUTPUT
