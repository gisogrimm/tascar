all: build ver lib symlink

include ../config.mk
include ../rules.mk

#
# versioning:
#
LIBVERSION=$(VERSION)
SOVERSION=$(shell echo $(VERSION) | sed 's/\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)/\1.\2/1')

#
# main targets:
#
OBJECTS = licensehandler.o audiostates.o coordinates.o audiochunks.o	\
  xmlconfig.o dynamicobjects.o sourcemod.o receivermod.o							\
  filterclass.o osc_helper.o pluginprocessor.o acousticmodel.o				\
  scene.o render.o session_reader.o session.o jackclient.o						\
  delayline.o async_file.o errorhandling.o osc_scene.o ringbuffer.o		\
  viewport.o sampler.o jackiowav.o cli.o irrender.o jackrender.o			\
  audioplugin.o levelmeter.o serviceclass.o alsamidicc.o							\
  speakerarray.o spectrum.o fft.o stft.o ola.o vbap3d.o hoa.o

DMXOBJECTS =  termsetbaud.o serialport.o dmxdriver.o

GUIOBJECTS = gui_elements.o pdfexport.o

# build dir:
BUILD_OBJECTS = $(patsubst %,build/%,$(OBJECTS))
BUILD_DMXOBJECTS = $(patsubst %,build/%,$(DMXOBJECTS))
BUILD_GUIOBJECTS = $(patsubst %,build/%,$(GUIOBJECTS))

ifeq "ok" "$(shell pkg-config gtkmm-3.0 && echo ok)"
GTKDEF = "-DGTKMM30"
GTKEXT = gtkmm-3.0
else
GTKDEF = "-DGTKMM24"
GTKEXT = gtkmm-2.4
endif

ifeq "$(DEBUG)" "yes"
CXXFLAGS += -ggdb -DTSCDEBUG
else
CXXFLAGS += -O3 
endif

ifeq "$(COVERAGE)" "yes"
CXXFLAGS += -coverage -fprofile-arcs -ftest-coverage
LDLIBS += -lgcov
endif

CXXFLAGS += $(GTKDEF) $(LTRDEF)

INSTBIN = $(patsubst %,$(PREFIX)/bin/%,$(BINFILES)) \
	$(patsubst %,$(PREFIX)/lib/%,$(RECEIVERMODS)) \
	$(patsubst %,$(PREFIX)/lib/%,$(SOURCEMODS)) \
	$(patsubst %,$(PREFIX)/lib/%,$(TASCARMODDLLS)) \
	$(patsubst %,$(PREFIX)/lib/%,$(AUDIOPLUGINDLLS))


CXXFLAGS += -fpic -I./build
CPPFLAGS += -I./build
#-L./ 
EXTERNALS = jack libxml++-2.6 liblo sndfile alsa fftw3f gsl eigen3

$(BUILD_GUIOBJECTS): EXTERNALS += gtkmm-3.0

LDLIBS += `pkg-config --libs $(EXTERNALS)`
CXXFLAGS += `pkg-config --cflags $(EXTERNALS)`

LDLIBS += -ldl
LDFLAGS += -L./

ver:
	echo '#define TASCARVER "'$(FULLVERSION)'"' > build/tascarver.h

lib:  $(BUILD_OBJECTS) build/libtascar.so.$(LIBVERSION)
	ldconfig -n build

symlink: build/libtascar.so

build/libtascar.so.%: $(BUILD_OBJECTS)
	$(CXX) -shared -Wl,-soname,libtascar.so.$(SOVERSION) -o $@ $^ ${LDFLAGS} ${LDLIBS}
	rm -f build/libtascar.so
#	ln -s $@ build/libtascar.so

build/libtascargui.so.%: $(BUILD_GUIOBJECTS)
	$(CXX) -shared -Wl,-soname,libtascargui.so.$(SOVERSION) -o $@ $^ ${LDFLAGS} ${LDLIBS}
	rm -f build/libtascargui.so
#	ln -s $@ build/libtascargui.so

build/libtascardmx.so.%: $(BUILD_DMXOBJECTS)
	$(CXX) -shared -Wl,-soname,libtascardmx.so.$(SOVERSION) -o $@ $^ ${LDFLAGS} ${LDLIBS}
	rm -f build/libtascardmx.so
#	ln -s $@ build/libtascardmx.so

clean:
	rm -Rf *~ src/*~ build doc/html

#VPATH = ../src

.PHONY : doc test gui

doc:
	cd doc && sed -e 's/PROJECT.NUMBER.*=.*/&'`cat ../version`'/1' doxygen.cfg > .temp.cfg && doxygen .temp.cfg
	rm -Rf doc/.temp.cfg

#include $(BUILD_OBJECTS:.o=.mk)

build/%.mk: src/%.cc
	$(CPP) $(CXXFLAGS) -MM -MF $(@:.o=.mk) $<
	printf "build/" > $(@).x
	cat $@ >> $(@).x
	mv $(@).x $@

%: %.o
	$(CXX) $(CXXFLAGS) $(LDLIBS) $^ -o $@

build/%.o: src/%.cc src/%.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/%.so: build/%.so.$(LIBVERSION)
	ln -s $(subst build/,,$<) $@

fullversion:
	echo $(FULLVERSION) >$@

# Local Variables:
# compile-command: "make"
# End:
