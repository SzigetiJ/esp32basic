AC_PREREQ([2.69])
AC_INIT([esp32basic],[1.0.0])

dnl Split PACKAGE_VERSION into MAJOR, MINOR and PATCH using shell parameter expansion
MAJOR_VERSION=`echo "$PACKAGE_VERSION" | cut -d . -f 1`
MINOR_VERSION=`echo "$PACKAGE_VERSION" | cut -d . -f 2`
PATCH_VERSION=`echo "$PACKAGE_VERSION" | cut -d . -f 3`

dnl Export them for use in Makefiles
AC_SUBST([MAJOR_VERSION])
AC_SUBST([MINOR_VERSION])
AC_SUBST([PATCH_VERSION])

dnl (Optional) Make them available as C preprocessor definitions #include "config.h"
AC_DEFINE_UNQUOTED([MAJOR_VERSION], [$MAJOR_VERSION], [Package major version])
AC_DEFINE_UNQUOTED([MINOR_VERSION], [$MINOR_VERSION], [Package minor version])
AC_DEFINE_UNQUOTED([PATCH_VERSION], [$PATCH_VERSION], [Package patch version])

AM_INIT_AUTOMAKE([gnu subdir-objects])
: ${CFLAGS="-g -Os -Wall -nostdlib -mlongcalls"} ${LDFLAGS="-specs=nano.specs"}
AC_PROG_CC
dnl AC_PROG_CC_STDC
AC_DEFUN([AC_PROG_AR], [AC_CHECK_TOOL(AR, ar, :)])
AC_PROG_AR
AC_PROG_RANLIB

AC_ARG_WITH([binaries], AS_HELP_STRING([--without-binaries], [Do not generate .bin images (esptool.py required).]))
AM_CONDITIONAL([WITH_BINARIES], [test x$with_binaries != xno])
AM_COND_IF([WITH_BINARIES],[ AC_MSG_NOTICE([generate .bin images]) ])

AC_CONFIG_HEADERS([src/e32b-config.h])
AC_CONFIG_FILES([
  Makefile
  src/Makefile
  modules/Makefile
  examples/Makefile
  examples/0blink/Makefile
  examples/0button/Makefile
  examples/0hello/Makefile
  examples/0ledctrl/Makefile
  examples/0timerint/Makefile
  examples/0timgalarm/Makefile
  examples/0uart/Makefile
  examples/0udma/Makefile
  examples/1bme280/Makefile
  examples/1i2cssd1306/Makefile
  examples/1rmtblink/Makefile
  examples/1rmtdht/Makefile
  examples/1rmtmorse/Makefile
  examples/1rmtmusic/Makefile
  examples/1rmttm1637/Makefile
  examples/1rmtws2812/Makefile
  examples/3prog1/Makefile
  ld/Makefile
])

AC_OUTPUT
