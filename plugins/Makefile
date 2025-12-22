all: build modules

include ../config.mk
include ../rules.mk

#-Wconversion -Werror

#
# main targets:
#
RECEIVERS = omni nsp amb3h0v amb3h3v amb1h0v amb1h1v cardioid hann	\
  vbap vbap3d hoa2d ortf intensityvector vmic chmap hoa2d_fuma		\
  cardioidmod debugpos hoa2d_fuma_hos fakebf wfs hoa3d hoa3d_enc	\
  simplefdn itu51 foaconv hrtf micarray stereo

SOURCES = omni cardioidmod door generic1storder farsrc head

TASCARMODS = system pos2osc sampler pendulum epicycles motionpath	\
  foa2hoadiff route oscevents oscjacktime nearsensor dirgain dummy	\
  matrix hoafdnrot mpu6050track touchosc savegains hrirconv		\
  hossustain rotator orientationmodulator locationmodulator		\
  linearmovement granularsynth fail waitforjackport sleep jackrec	\
  oscserver oscrelay echoc snapangle oscactor oscheadtracker osceog	\
  tonalenhancement systime skyfall accmovement accrotator tuner

TASCARMODSGUI = tracegui simplecontroller timedisplay geopresets	\
  transportgui lightcolorpicker datalogging sendsvg

AUDIOPLUGINS = delay dummy gate hannenv identity lipsync		\
  lipsync_paper lookatme metronome onsetdetector sine sndfile spksim	\
  gainramp pulse pink dumplevels feedbackdelay bandpass filter		\
  level2osc const noise loopmachine pos2osc gain sndfileasync		\
  addchannel tubesim spkcalib sessiontime flanger timestamp		\
  burglatticelpc fence transportramp allpass bandlevel2osc level2hsv	\
  ringmodulator reclevelanalyzer pareq4band sawtooth

MASKPLUGINS = fig8 multibeam sampledgain

ifneq ($(OS),Windows_NT)
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
  TASCARMODS += midicc2osc midictl mcuctl mididispatch joystick	\
    qualisystracker ltcgen lightctl ovheadtracker serialheadtracker
  AUDIOPLUGINS += simplesynth
endif
ifeq ($(UNAME_S),Darwin)
  TASCARMODS += ltcgen	 serialheadtracker\
#  ovheadtracker
endif
endif

ifeq "$(HAS_OPENCV2)" "yes"
GLABSENSORS += trackir
CXXFLAGS += -DHAS_OPENCV2
endif
ifeq "$(HAS_OPENCV4)" "yes"
GLABSENSORS += trackir
CXXFLAGS += -DHAS_OPENCV4
endif
# trackir requires opencv2, which is not available on Ubuntu 20.04


ifeq "$(HAS_LSL)" "yes"
CXXFLAGS += -DHAS_LSL
TASCARMODS += lsljacktime lslactor levels2osc pos2lsl osc2lsl
AUDIOPLUGINS += speechactivity periodogram
TASCARMODSGUI += waitforlslstream lsl2osc

ifneq ($(OS),Windows_NT)
# on Windows do not compile glabsensors:
TASCARMODSGUI += glabsensors
GLABSENSORS += smiley emergency espheadtracker	\
  qualisys jackstatus
endif

ifeq ($(UNAME_S),Linux)
GLABSENSORS += midicc serial oscthreshold eog
endif

endif

ifeq "$(HAS_OPENMHA)" "yes"
#AUDIOPLUGINS +=  openmha
endif

#
RECEIVERMODS = $(patsubst %,build/$(PLUGINPREFIX)tascarreceiver_%$(DLLEXT),$(RECEIVERS))

SOURCEMODS = $(patsubst %,build/$(PLUGINPREFIX)tascarsource_%$(DLLEXT),$(SOURCES))

TASCARMODDLLS = $(patsubst %,build/$(PLUGINPREFIX)tascar_%$(DLLEXT),$(TASCARMODS))
TASCARMODDLLSGUI = $(patsubst %,build/$(PLUGINPREFIX)tascar_%$(DLLEXT),$(TASCARMODSGUI))

AUDIOPLUGINDLLS = $(patsubst %,build/$(PLUGINPREFIX)tascar_ap_%$(DLLEXT),$(AUDIOPLUGINS))

MASKPLUGINDLLS = $(patsubst %,build/$(PLUGINPREFIX)tascar_mask_%$(DLLEXT),$(MASKPLUGINS))

GLABSENSORDLLS = $(patsubst %,build/$(PLUGINPREFIX)glabsensor_%$(DLLEXT),$(GLABSENSORS))


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

TASCARLIB = -ltascar
TASCARDMXLIB = -ltascardmx
TASCARGUILIB = -ltascargui

CXXFLAGS += $(GTKDEF) $(LTRDEF)
# add libtascar/src for main headers, and libtascar/build for version number:
CXXFLAGS += -I../libtascar/include -I../libtascar/build
LDFLAGS += -L../libtascar/build
CXXFLAGS += -DTASCARVERSION="\"$(TASCARVERSION)\""

CXXFLAGS += -fpic
EXTERNALS = jack xerces-c liblo sndfile fftw3f samplerate

LDLIBS += `pkg-config --libs $(EXTERNALS)`
CXXFLAGS += `pkg-config --cflags $(EXTERNALS)`

LDLIBS += -ldl $(TASCARLIB)

modules: $(RECEIVERMODS) $(SOURCEMODS) $(TASCARMODDLLS) $(TASCARMODDLLSGUI) $(AUDIOPLUGINDLLS) $(MASKPLUGINDLLS) $(GLABSENSORDLLS)

clean:
	rm -Rf *~ src/*~ build doc/html

.PHONY : doc

$(TASCARMODDLLSGUI) $(GLABSENSORDLLS): EXTERNALS += gtkmm-3.0
$(TASCARMODDLLSGUI) $(GLABSENSORDLLS): CXXFLAGS += -I./build
$(TASCARMODDLLSGUI) $(GLABSENSORDLLS): LDLIBS += $(TASCARGUILIB)

ifeq ($(OS),Windows_NT)
$(TASCARMODDLLSGUI) $(GLABSENSORDLLS): LDLIBS += -lsigc-2.0
endif


build/$(PLUGINPREFIX)glabsensor_%$(DLLEXT): EXTERNALS += gtkmm-3.0
build/$(PLUGINPREFIX)glabsensor_%$(DLLEXT): LDLIBS+=-llsl
build/$(PLUGINPREFIX)tascar_ap_lipsync$(DLLEXT): LDLIBS+=-lfftw3f
build/$(PLUGINPREFIX)tascar_ap_lipsync_paper$(DLLEXT): LDLIBS+=-lfftw3f
build/$(PLUGINPREFIX)tascar_ap_openmha$(DLLEXT): LDLIBS+=-lopenmha
build/$(PLUGINPREFIX)tascar_ap_periodogram$(DLLEXT): LDLIBS+=-llsl
build/$(PLUGINPREFIX)tascar_ap_speechactivity$(DLLEXT): LDLIBS+=-llsl
build/$(PLUGINPREFIX)tascar_lsl2osc$(DLLEXT): LDLIBS+=-llsl
build/$(PLUGINPREFIX)tascar_datalogging$(DLLEXT): LDLIBS += -lmatio
ifeq "$(HAS_LSL)" "yes"
build/$(PLUGINPREFIX)tascar_datalogging$(DLLEXT):  LDLIBS += -llsl
endif
build/$(PLUGINPREFIX)tascar_datalogging$(DLLEXT): build/datalogging_glade.h
build/$(PLUGINPREFIX)tascar_hossustain$(DLLEXT): EXTERNALS += fftw3f
build/$(PLUGINPREFIX)tascar_lightctl$(DLLEXT) build/$(PLUGINPREFIX)tascar_ovheadtracker$(DLLEXT) build/$(PLUGINPREFIX)tascar_serialheadtracker$(DLLEXT): LDLIBS+=$(TASCARDMXLIB)
build/$(PLUGINPREFIX)tascar_lsljacktime$(DLLEXT) build/$(PLUGINPREFIX)tascar_pos2lsl$(DLLEXT) build/$(PLUGINPREFIX)tascar_levels2osc$(DLLEXT) build/$(PLUGINPREFIX)tascar_lslactor$(DLLEXT) build/$(PLUGINPREFIX)tascar_osc2lsl$(DLLEXT): LDLIBS+=-llsl
build/$(PLUGINPREFIX)tascar_ltcgen$(DLLEXT): EXTERNALS+=ltc
build/$(PLUGINPREFIX)tascar_simplecontroller$(DLLEXT): build/simplecontroller_glade.h
build/$(PLUGINPREFIX)tascar_timedisplay$(DLLEXT): build/timedisplay_glade.h
build/$(PLUGINPREFIX)tascar_waitforlslstream$(DLLEXT): LDLIBS+=-llsl
build/$(PLUGINPREFIX)tascarreceiver_hoa2d$(DLLEXT): LDLIBS+=-lfftw3f
build/$(PLUGINPREFIX)tascarreceiver_hoa3d$(DLLEXT): EXTERNALS+=gsl eigen3
build/$(PLUGINPREFIX)tascarreceiver_hoa3d_enc$(DLLEXT): EXTERNALS+=gsl

ifeq "$(HAS_OPENCV2)" "yes"
build/$(PLUGINPREFIX)glabsensor_trackir$(DLLEXT): EXTERNALS += opencv
endif
ifeq "$(HAS_OPENCV4)" "yes"
build/$(PLUGINPREFIX)glabsensor_trackir$(DLLEXT): EXTERNALS += opencv4
endif

build/$(PLUGINPREFIX)tascar_transportgui$(DLLEXT): build/transportgui_glade.h

build/glabsensors.o: EXTERNALS += gtkmm-3.0
build/glabsensors.o: build/glabsensors_glade.h
build/$(PLUGINPREFIX)tascar_glabsensors$(DLLEXT): build/glabsensors_glade.h
build/$(PLUGINPREFIX)tascar_glabsensors$(DLLEXT): build/glabsensorplugin.o build/glabsensors_glade.h src/glabsensorplugin.h
build/$(PLUGINPREFIX)tascar_glabsensors$(DLLEXT): LDLIBS += -llsl build/glabsensorplugin.o

build/$(PLUGINPREFIX)tascarreceiver_%$(DLLEXT): src/receivermod_%.cc
	$(CXX) -shared -o $@ $< $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

build/$(PLUGINPREFIX)tascarsource_%$(DLLEXT): src/tascarsource_%.cc
	$(CXX) -shared -o $@ $< $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

build/$(PLUGINPREFIX)tascar_%$(DLLEXT): src/tascarmod_%.cc
	$(CXX) -shared -o $@ $< $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

build/$(PLUGINPREFIX)tascar_ap_%$(DLLEXT): src/tascar_ap_%.cc
	$(CXX) -shared -o $@ $< $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

build/$(PLUGINPREFIX)tascar_mask_%$(DLLEXT): src/tascar_mask_%.cc
	$(CXX) -shared -o $@ $< $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

build/$(PLUGINPREFIX)glabsensor_%$(DLLEXT): src/glabsensor_%.cc build/glabsensorplugin.o
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
