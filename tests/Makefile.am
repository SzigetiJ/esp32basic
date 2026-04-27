TESTS= \
 bytegen_test \
 bitgen_test

check_PROGRAMS = $(TESTS)

AM_CPPFLAGS = -I${top_srcdir}/../src
LDADD = gentest_common.o

bitgen_test_SOURCES = bitgen_test.c ${top_srcdir}/../src/utils/generators.c
bytegen_test_SOURCES = bytegen_test.c ${top_srcdir}/../src/utils/generators.c
