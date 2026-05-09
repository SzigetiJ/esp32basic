TESTS= \
 bytegen_test \
 bitgen_test \
 bme280_test \
 exptuner_test

check_PROGRAMS = $(TESTS)

AM_CPPFLAGS = -I${top_srcdir}/../src -I${top_srcdir}/../modules -I${top_srcdir}/../examples
bitgen_test_LDADD = gentest_common.o
bytegen_test_LDADD = gentest_common.o

bitgen_test_SOURCES = bitgen_test.c ../src/utils/generators.c
bytegen_test_SOURCES = bytegen_test.c ../src/utils/generators.c
bme280_test_SOURCES = bme280_test.c ../modules/bme280.c mocks/i2c_mock.c mocks/lockmgr_mock.c
exptuner_test_SOURCES = exptuner_test.c ../examples/1bme280/exptuner.c