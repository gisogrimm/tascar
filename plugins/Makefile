all: build modules

include ../config.mk
include ../rules.mk

#
# main targets:
#
RECEIVERS = omni ortf simplefdn itu51 hrtf

SOURCES = omni

TASCARMODS = system route touchosc savegains oscserver	\
   waitforjackport sleep

#TASCARMODSGUI = tracegui simplecontroller timedisplay geopresets

AUDIOPLUGINS = delay identity metronome sine sndfile pink bandpass	\
   filter loopmachine gain sndfileasync

ifeq "$(HAS_OPENCV2)" "yes"
#GLABSENSORS += trackir
#CXXFLAGS += -DHAS_OPENCV2
endif
ifeq "$(HAS_OPENCV4)" "yes"
#GLABSENSORS += trackir
#CXXFLAGS += -DHAS_OPENCV4
endif
# trackir requires opencv2, which is not available on Ubuntu 20.04


ifeq "$(HAS_LSL)" "yes"
#TASCARMODS +=  lsljacktime lslactor levels2osc pos2lsl
#AUDIOPLUGINS += speechactivity periodogram
#TASCARMODSGUI += datalogging glabsensors
#GLABSENSORS += smiley eog emergency espheadtracker midicc	\
#  qualisys jackstatus serial
endif

ifeq "$(HAS_OPENMHA)" "yes"
#AUDIOPLUGINS +=  openmha
endif

#
RECEIVERMODS = $(patsubst %,build/tascarreceiver_%.so,$(RECEIVERS))

SOURCEMODS = $(patsubst %,build/tascarsource_%.so,$(SOURCES))

TASCARMODDLLS = $(patsubst %,build/tascar_%.so,$(TASCARMODS))
TASCARMODDLLSGUI = $(patsubst %,build/tascar_%.so,$(TASCARMODSGUI))

AUDIOPLUGINDLLS = $(patsubst %,build/tascar_ap_%.so,$(AUDIOPLUGINS))

GLABSENSORDLLS = $(patsubst %,build/glabsensor_%.so,$(GLABSENSORS))


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

TASCARVERSION=$(FULLVERSION)

CXXFLAGS += $(GTKDEF) $(LTRDEF)
# add libtascar/src for main headers, and libtascar/build for version number:
CXXFLAGS += -I../libtascar/src -I../libtascar/build
LDFLAGS += -L../libtascar/build
CXXFLAGS += -DTASCARVERSION="\"$(TASCARVERSION)\""

CXXFLAGS += -fpic
EXTERNALS = jack libxml++-2.6 liblo sndfile

LDLIBS += `pkg-config --libs $(EXTERNALS)`
CXXFLAGS += `pkg-config --cflags $(EXTERNALS)`

LDLIBS += -ldl -ltascar

modules: $(RECEIVERMODS) $(SOURCEMODS) $(TASCARMODDLLS) $(TASCARMODDLLSGUI) $(AUDIOPLUGINDLLS) $(GLABSENSORDLLS)

clean:
	rm -Rf *~ src/*~ build doc/html

.PHONY : doc

$(TASCARMODDLLSGUI): EXTERNALS += gtkmm-3.0
$(TASCARMODDLLSGUI): CXXFLAGS+=-I./build

build/tascarreceiver_hoa2d.so: LDLIBS+=-lfftw3f
build/tascar_ap_lipsync.so: LDLIBS+=-lfftw3f
build/tascar_ap_lipsync_paper.so: LDLIBS+=-lfftw3f
build/tascar_ap_speechactivity.so: LDLIBS+=-llsl
build/tascar_ap_periodogram.so: LDLIBS+=-llsl
build/tascar_ap_openmha.so: LDLIBS+=-lopenmha
build/tascar_ltcgen.so: EXTERNALS+=ltc
build/tascarreceiver_hoa3d_enc.so: EXTERNALS+=gsl
build/tascarreceiver_hoa3d.so: EXTERNALS+=gsl eigen3
build/tascar_hossustain.so: EXTERNALS += fftw3f
build/tascar_lsljacktime.so build/tascar_pos2lsl.so build/tascar_levels2osc.so build/tascar_lslactor.so: LDLIBS+=-llsl
build/tascar_lightctl.so: LDLIBS+=-ltascardmx
build/tascar_simplecontroller.so: build/simplecontroller_glade.h
build/tascar_datalogging.so: LDLIBS += -lmatio -llsl
build/tascar_datalogging.so: build/datalogging_glade.h
build/tascar_timedisplay.so: build/timedisplay_glade.h

build/glabsensor_%.so: EXTERNALS += gtkmm-3.0
build/glabsensor_%.so: LDLIBS += -llsl

ifeq "$(HAS_OPENCV2)" "yes"
build/glabsensor_trackir.so: EXTERNALS += opencv
endif
ifeq "$(HAS_OPENCV4)" "yes"
build/glabsensor_trackir.so: EXTERNALS += opencv4
endif

build/glabsensors.o: EXTERNALS += gtkmm-3.0
build/glabsensors.o: build/glabsensors_glade.h
build/tascar_glabsensors.so: build/glabsensors_glade.h
build/tascar_glabsensors.so: build/glabsensorplugin.o build/glabsensors_glade.h src/glabsensorplugin.h
build/tascar_glabsensors.so: LDLIBS += -llsl

build/tascarreceiver_%.so: src/receivermod_%.cc
	$(CXX) -shared -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

build/tascarsource_%.so: src/tascarsource_%.cc
	$(CXX) -shared -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

build/tascar_%.so: src/tascarmod_%.cc
	$(CXX) -shared -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

build/tascar_ap_%.so: src/tascar_ap_%.cc
	$(CXX) -shared -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

build/glabsensor_%.so: src/glabsensor_%.cc build/glabsensorplugin.o src/glabsensorplugin.h
	$(CXX) -shared -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

build/%_glade.h: src/%.glade
	cat $< | sed 's/tascarversion/$(TASCARVERSION)/g' > $*_glade
	echo "#include <string>" > $@
	xxd -i $*_glade >> $@
	echo "std::string ui_"$*"((const char*)"$*"_glade,(size_t)"$*"_glade_len);" >> $@
	rm -f $*_glade

build/%.o: src/%.cc
	$(CXX) $(CXXFLAGS) -o $@ -c $^


# Local Variables:
# compile-command: "make"
# End:
