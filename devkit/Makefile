 # Makefile for TASCAR modules

# general purpose and actor modules:
MODULES += tascar_rotate

# receiver modules:
MODULES += tascarreceiver_example

# source modules:
MODULES += tascarsource_example

# audio processing plugins:
MODULES += tascar_ap_noise

# list of test files
TESTFILES = $(shell find ./test/ -name "*.tsc")

all: build modules

# include compiler configuration to ensure ABI compatibility:
include /usr/share/tascar/config.mk

# include TASCAR specific make rules:
include /usr/share/tascar/rules.mk

EXTERNALS=xerces-c

LDLIBS += `pkg-config --libs $(EXTERNALS)`
CXXFLAGS += `pkg-config --cflags $(EXTERNALS)`
CXXFLAGS += -Wno-unused-parameter

LDLIBS += -ltascar

# build targets:
BUILDMODS = $(patsubst %,build/%.so,$(MODULES))

# install targets:
INSTMODS = $(patsubst %,$(PREFIX)/lib/%.so,$(MODULES))

modules: $(BUILDMODS)

install: $(INSTMODS)

uninstall:
	rm -f $(INSTMOD)

clean:
	rm -Rf *~ src/*~ build
	find . -name "*.jvalid" -exec rm -f \{\} \;
	find . -name "*.validated" -exec rm -f \{\} \;

$(PREFIX)/lib/%: build/%
	cp $< $@

build/tascar_ap_openmha.so: LDLIBS+=-lopenmha

build/%.so: src/%.cc
	$(CXX) -shared -fpic -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

%.validated: %.tsc $(BUILDMODS)
	LD_LIBRARY_PATH=$${LD_LIBRARY_PATH}:$(PWD)/build tascar_validatetsc $< || (echo "$(<):1:";false)
	echo ok > $@

%.jvalid: %.tsc $(BUILDMODS)
	LD_LIBRARY_PATH=$${LD_LIBRARY_PATH}:$(PWD)/build tascar_cli -v $<  || (echo "$(<):1:";false)
	echo ok > $@

test: $(TESTFILES:.tsc=.validated)

testjack:
	jackd -d dummy -C 64 -P 64 -r 44100 -p 1024 &
	sleep 1
	$(MAKE) $(TESTFILES:.tsc=.jvalid) || (killall jackd;false)
	killall jackd

clean:


# Local Variables:
# compile-command: "make"
# End:
