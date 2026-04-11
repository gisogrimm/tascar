all: build bin

include ../config.mk
include ../rules.mk

#
# main targets:
#
BINFILES = tascar_cli tascar_showlicenses tascar_validatetsc		\
  tascar_pdf tascar_osc_jack_transport tascar_jackio tascar_sampler	\
  tascar_levelmeter tascar_jackpar tascar_renderfile tascar_renderir	\
  tascar_gpx2csv tascar_version tascar_test_compare_sndfile		\
  tascar_test_compare_level_sum tascar_lsjackp tascar_sendosc		\
  tascar_listsrc tascar_getcalibfor tascar_spk2obj			\
  tascar_sceneskeleton tascar_osc2file tascar_osc2csv

ifneq ($(OS),Windows_NT)
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
  BINFILES += tascar_hdspmixer
endif
endif

TEST_FILES =
#test_ngon test_sinc

BINFILES += $(TEST_FILES)

BUILD_BINS = $(patsubst %,build/%,$(BINFILES))

ifeq "$(HAS_LSL)" "yes"
BINFILES += tascar_lslsl tascar_lsljacktime tascar_osc2lsl tascar_genrandlsl
endif

ifeq "$(HAS_MOSQUITTO)" "yes"
ifeq "$(HAS_NLOHMANN)" "yes"
BINFILES += tascar_mqtt2osc
endif
endif

ifeq "ok" "$(shell pkg-config gtkmm-3.0 && echo ok)"
GTKDEF = "-DGTKMM30"
GTKEXT = gtkmm-3.0
else
GTKDEF = "-DGTKMM24"
GTKEXT = gtkmm-2.4
endif

ifeq "$(DEBUG)" "yes"
CXXFLAGS += -ggdb
else
CXXFLAGS += -O3 
endif

ifeq "$(COVERAGE)" "yes"
CXXFLAGS += -coverage -fprofile-arcs -ftest-coverage -fPIC
endif

CXXFLAGS += $(GTKDEF) $(LTRDEF)
# add libtascar/src for main headers, and libtascar/build for version number:
CXXFLAGS += -I../libtascar/include -I../libtascar/build
LDFLAGS += -L../libtascar/build 

LDLIBS += -ldl -ltascar -lfftw3f

EXTERNALS = jack liblo sndfile xerces-c fftw3
EXTERNALLIBS := $(shell pkg-config --libs $(EXTERNALS))
EXTERNALCFLAGS := $(shell pkg-config --cflags $(EXTERNALS))
ALSALIBS := $(shell pkg-config --libs alsa)
ALSACFLAGS := $(shell pkg-config --cflags alsa)
GTKCFLAGS := $(shell pkg-config --cflags $(GTKEXT))
GTKLIBS := $(shell pkg-config --libs $(GTKEXT))

LDLIBS += $(EXTERNALLIBS)
CXXFLAGS += $(EXTERNALCFLAGS)

bin: $(BUILD_BINS)

build/tascar_hoadisplay: LDLIBS += $(GTKLIBS)
build/tascar_hoadisplay: CXXFLAGS += $(GTKCFLAGS)
build/tascar_hdspmixer build/tascar_hdspmixer.o: LDLIBS += $(ALSALIBS)
build/tascar_hdspmixer build/tascar_hdspmixer.o: CXXFLAGS += $(ALSACFLAGS)
build/tascar_pdf build/tascar_pdf.o: CXXFLAGS += $(GTKCFLAGS)
build/tascar_pdf build/tascar_pdf.o: LDLIBS += -ltascargui $(EXTERNALLIBS) $(GTKLIBS)
build/tascar_ambdecoder: LDLIBS += `pkg-config --libs gsl`
build/tascar_lslsl build/tascar_lsljacktime build/tascar_osc2lsl build/tascar_genrandlsl: LDLIBS+= $(LSLLIBS)
build/tascar_mqtt2osc: LDLIBS += -lmosquitto

ifeq "$(COVERAGE)" "yes"
LDLIBS += -lgcov -lgmock_main
endif

clean:
	rm -Rf *~ src/*~ build doc/html

include $(wildcard build/*.mk)

$(PREFIX)/bin/%: %
	cp $< $@

build/%: build/%.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

# The additional build/.directory dependency ensures subdirectory creation
build/%.o: src/%.cc build/.directory
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Local Variables:
# compile-command: "make"
# End:
