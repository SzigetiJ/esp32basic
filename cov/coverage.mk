SELF_MKFILE := $(lastword $(MAKEFILE_LIST))

PROJ=esp32basic

##progs
GCOV=gcov

## paths
HTML=html/index.html
SRCDIR=../src
MODDIR=../modules
EGDIR=../examples

## we get direct information about modification time
## note: I do not like wildcard, but could not find better solution
SOURCES_SRC=$(wildcard $(SRCDIR)/*.c) $(wildcard $(SRCDIR)/**/*.c)
HEADERS_SRC=$(wildcard $(SRCDIR)/*.h)$(wildcard $(SRCDIR)/**/*.h)
OBJS_SRC_PRE=$(SOURCES_SRC:%.c=%.o)
OBJS_SRC=$(OBJS_SRC_PRE:../%=%)

SOURCES_MODULES=$(wildcard $(MODDIR)/*.c) $(wildcard $(MODDIR)/**/*.c)
HEADERS_MODULES=$(wildcard $(MODDIR)/*.h) $(wildcard $(MODDIR)/**/*.h)
OBJS_MODULES_PRE=$(SOURCES_MODULES:%.c=%.o)
OBJS_MODULES=$(OBJS_MODULES_PRE:../%=%)

SOURCES_EXAMPLES=$(wildcard $(EGDIR)/*.c) $(wildcard $(EGDIR)/**/*.c)
HEADERS_EXAMPLES=$(wildcard $(EGDIR)/*.h) $(wildcard $(EGDIR)/**/*.h)
OBJS_EXAMPLES_PRE=$(SOURCES_EXAMPLES:%.c=%.o)
OBJS_EXAMPLES=$(OBJS_EXAMPLES_PRE:../%=%)


GCDA=$(OBJS_SRC:.o=.gcda) $(OBJS_MODULES:.o=.gcda) $(OBJS_EXAMPLES:.o=.gcda)
GCNO=$(OBJS_SRC:.o=.gcno) $(OBJS_MODULES:.o=.gcno) $(OBJS_EXAMPLES:.o=.gcno)

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

examples/%.c.gcov: examples/%.gcno
	cd examples && $(GCOV) -wrabcfu -s ../.. $(<:examples/%.gcno=%.o)
	[ -f $@ ] || mv examples/$(notdir $@) $@

$(HTML): $(PROJ).info
	genhtml -s --branch-coverage $(PROJ).info --output-directory $(dir $(HTML))

$(PROJ).info: $(PROJ).pre.info
	lcov --ignore-errors unused --rc lcov_branch_coverage=1 -r $< "/usr*" -o $@

$(PROJ).pre.info: $(PROJ).base.info $(PROJ).test.info
	lcov --rc lcov_branch_coverage=1 -a $(PROJ).base.info -a $(PROJ).test.info -o $@

$(PROJ).base.info: testrun
	lcov -z -d src -d modules
	lcov --rc lcov_branch_coverage=1 -c -i -d src -d modules -d examples -o $@

$(PROJ).test.info: $(PROJ).base.info
	$(MAKE) -C tests check
	lcov --rc lcov_branch_coverage=1 -c -d src -d modules -d examples -o $@


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
	rm -f examples/*.gcda
	rm -f tests/*.gcda
	$(MAKE) -C tests clean

distclean: clean
	$(MAKE) -C tests distclean
	rm -rf src
	rm -rf modules
	rm -rf examples
	rm -rf tests

.PHONY: clean distclean source testrun gcov _gcov
