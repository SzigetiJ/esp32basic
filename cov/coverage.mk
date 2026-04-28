SELF_MKFILE := $(lastword $(MAKEFILE_LIST))

PROJ=esp32basic

##progs
GCOV=gcov

## paths
HTML=html/index.html
SRCDIR=../src
## we get direct information about modification time
## note: I do not like wildcard, but could not find better solution
SOURCES=$(wildcard $(SRCDIR)/*.c)
HEADERS=$(wildcard $(SRCDIR)/*.h)
OBJS=$(addprefix src/, $(notdir $(SOURCES:.c=.o)))
GCDA=$(OBJS:.o=.gcda)
GCNO=$(OBJS:.o=.gcno)
GCDA_EXIST := $(foreach gcda,$(GCDA),$(wildcard $(gcda)))
#$(info $$GCDA = $(GCDA))
#$(info $$GCDA_EXIST = $(GCDA_EXIST))

all: $(HTML)

gcov: testrun
	$(MAKE) -f $(SELF_MKFILE) _gcov

_gcov: $(GCNO:%.gcno=%.c.gcov)

%.c.gcov: %.gcno
	cd src; $(GCOV) -wrabcfu -s ../.. -o $(notdir $<) $(notdir $(@:.gcov=))

#src/%.gcda: testrun


$(HTML): $(PROJ).info
	genhtml -s --branch-coverage $(PROJ).info --output-directory $(dir $(HTML))

$(PROJ).info: $(PROJ).pre.info
	lcov --rc lcov_branch_coverage=1 -r $< "/usr*" -o $@

$(PROJ).pre.info: $(PROJ).base.info $(PROJ).test.info
	lcov --rc lcov_branch_coverage=1 -a $(PROJ).base.info -a $(PROJ).test.info -o $@

#$(PROJ).base.info: src/lib$(PROJ).a
$(PROJ).base.info: testrun
	lcov -z -d src
	lcov --rc lcov_branch_coverage=1 -c -i -d src -o $@

$(PROJ).test.info: $(PROJ).base.info
	$(MAKE) -C tests check
	lcov --rc lcov_branch_coverage=1 -c -d src -o $@


#source: Makefile FORCE
#	$(MAKE) -C src

#testrun: source FORCE
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
	rm -f tests/*.gcda
	$(MAKE) -C tests clean

distclean: clean
	$(MAKE) -C tests distclean
	rm -rf src
	rm -rf tests

.PHONY: clean distclean source testrun gcov _gcov

