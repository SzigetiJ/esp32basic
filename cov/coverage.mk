SELF_MKFILE := $(lastword $(MAKEFILE_LIST))

PROJ=esp32basic

##progs
GCOV=gcov

## paths
HTML=html/index.html
SRCDIR=../src
MODDIR=../modules

## we get direct information about modification time
## note: I do not like wildcard, but could not find better solution
SOURCES=$(wildcard $(SRCDIR)/*.c)
HEADERS=$(wildcard $(SRCDIR)/*.h)
OBJS=$(addprefix src/, $(notdir $(SOURCES:.c=.o)))

SOURCES_UTILS=$(wildcard $(SRCDIR)/utils/*.c)
HEADERS_UTILS=$(wildcard $(SRCDIR)/utils/*.h)
OBJS_UTILS=$(addprefix src/utils/, $(notdir $(SOURCES_UTILS:.c=.o)))

SOURCES_MODULES=$(wildcard $(MODDIR)/*.c)
HEADERS_MODULES=$(wildcard $(MODDIR)/*.h)
OBJS_MODULES=$(addprefix modules/, $(notdir $(SOURCES_MODULES:.c=.o)))

GCDA=$(OBJS:.o=.gcda) $(OBJS_UTILS:.o=.gcda) $(OBJS_MODULES:.o=.gcda)
GCNO=$(OBJS:.o=.gcno) $(OBJS_UTILS:.o=.gcno) $(OBJS_MODULES:.o=.gcno)

GCDA_EXIST := $(foreach gcda,$(GCDA),$(wildcard $(gcda)))
GCNO_EXIST := $(foreach gcno,$(GCNO),$(wildcard $(gcno)))

$(info $$GCNO = $(GCNO))
$(info $$GCNO_EXIST = $(GCNO_EXIST))

all: $(HTML)

gcov: testrun
	$(MAKE) -f $(SELF_MKFILE) _gcov

_gcov: $(GCNO_EXIST:%.gcno=%.c.gcov)

src/%.c.gcov: src/%.gcno
	cd src && $(GCOV) -wrabcfu -s ../.. $(<:src/%.gcno=%.o)
	[ -f $@ ] || mv src/$(notdir $@) $@

modules/%.c.gcov: modules/%.gcno
	cd modules && $(GCOV) -wrabcfu -s ../.. $(<:modules/%.gcno=%.o)
	[ -f $@ ] || mv modules/$(notdir $@) $@

$(HTML): $(PROJ).info
	genhtml -s --branch-coverage $(PROJ).info --output-directory $(dir $(HTML))

$(PROJ).info: $(PROJ).pre.info
	lcov --ignore-errors unused --rc lcov_branch_coverage=1 -r $< "/usr*" -o $@

$(PROJ).pre.info: $(PROJ).base.info $(PROJ).test.info
	lcov --rc lcov_branch_coverage=1 -a $(PROJ).base.info -a $(PROJ).test.info -o $@

$(PROJ).base.info: testrun
	lcov -z -d src -d modules
	lcov --rc lcov_branch_coverage=1 -c -i -d src -d modules -o $@

$(PROJ).test.info: $(PROJ).base.info
	$(MAKE) -C tests check
	lcov --rc lcov_branch_coverage=1 -c -d src -d modules -o $@


testrun: tests/Makefile FORCE
	$(MAKE) -C tests check

tests/Makefile:
	mkdir -p tests
	(cd tests && ../../tests/configure CFLAGS="-fprofile-arcs -ftest-coverage" LDFLAGS="-fprofile-arcs -ftest-coverage")

FORCE:

clean:
	rm -f $(PROJ).info
	rm -f $(PROJ).pre.info
	rm -f $(PROJ).base.info
	rm -f $(PROJ).test.info
	rm -rf $(dir $(HTML))
	rm -f src/*.gcda
	rm -f modules/*.gcda
	rm -f tests/*.gcda
	$(MAKE) -C tests clean

distclean: clean
	$(MAKE) -C tests distclean
	rm -rf src
	rm -rf modules
	rm -rf tests

.PHONY: clean distclean source testrun gcov _gcov
