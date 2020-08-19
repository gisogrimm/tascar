PREFIX=/usr/local
LIBDIR=$(PREFIX)/lib
BINDIR=$(PREFIX)/bin
INCDIR=$(PREFIX)/include
DESTDIR=

MODULES = libtascar plugins
DOCMODULES = doc manual

all: $(MODULES)

alldoc: all $(DOCMODULES)

.PHONY : $(MODULES) $(DOCMODULES) coverage

$(MODULES:external_libs=) $(DOCMODULES):
	$(MAKE) -C $@

clean:
	for m in $(MODULES) $(DOCMODULES); do $(MAKE) -C $$m clean; done
	$(MAKE) -C test clean
	rm -Rf build devkit/Makefile.local devkit/build

test:
	$(MAKE) -C test
	$(MAKE) -C examples

googletest:
	$(MAKE) -C external_libs googlemock

unit-tests: $(patsubst %,%-subdir-unit-tests,$(MODULES))
$(patsubst %,%-subdir-unit-tests,$(MODULES)): all googletest
	$(MAKE) -C $(@:-subdir-unit-tests=) unit-tests

coverage: googletest unit-tests
	lcov --capture --directory ./ --output-file coverage.info
	genhtml coverage.info --prefix $$PWD --show-details --demangle-cpp --output-directory $@
	x-www-browser ./coverage/index.html

install: all
	install -D libtascar/build/libtascar*.so -t $(DESTDIR)$(LIBDIR)
	install -D libtascar/src/*.h -t $(DESTDIR)$(INCDIR)/tascar
	install -D libtascar/build/*.h -t $(DESTDIR)$(INCDIR)/tascar
	install -D plugins/build/*.so -t $(DESTDIR)$(LIBDIR)
	ldconfig -n $(DESTDIR)$(LIBDIR)

.PHONY : all clean test

