#
# versioning:
#

GITMODIFIED=$(shell test -z "`git status --porcelain -uno`" || echo "-modified")
COMMITHASH=$(shell git log -1 --abbrev=7 --pretty='format:%h')
FULLVERSION=$(shell cat version)-$(COMMITHASH)$(GITMODIFIED)
DEFFULLVERSION=$(shell echo "\#define TASCARVER \\\""$(FULLVERSION)"\\\"")

#
# main targets:
#
BINFILES = tascar_cli tascar_tscupdate tascar_showlicenses tascar_pdf	\
  tascar_osc_jack_transport tascar_jackio tascar_sampler		\
  tascar_hdspmixer tascar_levelmeter tascar_jackpar tascar_lslsl	\
  tascar_lsljacktime tascar_renderfile tascar_renderir tascar_gpx2csv	\
  tascar_version

RECEIVERS = omni nsp amb3h0v amb3h3v amb1h0v amb1h1v cardioid	\
  neukom_basic neukom_inphase hann vbap vbap3d hoa2d ortf	\
  intensityvector vmic chmap hoa2d_fuma cardioidmod debugpos	\
  hoa2d_fuma_hos fakebf

SOURCES = omni cardioidmod door

TASCARMODS = system pos2osc sampler pendulum epicycles motionpath	\
  foa2hoadiff route lsljacktime oscevents oscjacktime ltcgen		\
  artnetdmx opendmxusb lightctl levels2osc nearsensor dirgain dummy	\
  matrix hoafdnrot mpu6050track touchosc savegains lslactor

OBJECTS = licensehandler.o audiostates.o coordinates.o audiochunks.o	\
  xmlconfig.o dynamicobjects.o sourcemod.o receivermod.o		\
  filterclass.o osc_helper.o acousticmodel.o scene.o render.o		\
  session_reader.o session.o jackclient.o delayline.o async_file.o	\
  errorhandling.o osc_scene.o ringbuffer.o viewport.o sampler.o		\
  jackiowav.o cli.o irrender.o jackrender.o audioplugin.o		\
  levelmeter.o serviceclass.o alsamidicc.o speakerarray.o spectrum.o	\
  fft.o stft.o ola.o termsetbaud.o serialport.o dmxdriver.o

AUDIOPLUGINS = identity sine lipsync lipsync_paper lookatme		\
  onsetdetector delay sndfile spksim dummy hannenv gate metronome

TEST_FILES = test_ngon test_sinc test_fsplit compare_sndfile

#
RECEIVERMODS = $(patsubst %,tascarreceiver_%.so,$(RECEIVERS))

SOURCEMODS = $(patsubst %,tascarsource_%.so,$(SOURCES))

TASCARMODDLLS = $(patsubst %,tascar_%.so,$(TASCARMODS))

AUDIOPLUGINDLLS = $(patsubst %,tascar_ap_%.so,$(AUDIOPLUGINS))

PREFIX = /usr/local

BINFILES += $(TEST_FILES)

ifeq "ok" "$(shell pkg-config gtkmm-3.0 && echo ok)"
GTKDEF = "-DGTKMM30"
GTKEXT = gtkmm-3.0
else
GTKDEF = "-DGTKMM24"
GTKEXT = gtkmm-2.4
endif

ifeq "ok" "$(shell test -e /usr/include/linuxtrack.h && echo  ok)"
LTRDEF = "-DLINUXTRACK"
LDLIBS += -llinuxtrack
endif

ifeq "$(DEBUG)" "yes"
CXXFLAGS += -ggdb -DTSCDEBUG
else
CXXFLAGS += -O3 
endif

CXXFLAGS += $(GTKDEF) $(LTRDEF)
CXXFLAGS += -fext-numeric-literals 

GUIOBJECTS = gui_elements.o pdfexport.o

INSTBIN = $(patsubst %,$(PREFIX)/bin/%,$(BINFILES)) \
	$(patsubst %,$(PREFIX)/lib/%,$(RECEIVERMODS)) \
	$(patsubst %,$(PREFIX)/lib/%,$(SOURCEMODS)) \
	$(patsubst %,$(PREFIX)/lib/%,$(TASCARMODDLLS)) \
	$(patsubst %,$(PREFIX)/lib/%,$(AUDIOPLUGINDLLS))


CXXFLAGS += -fPIC -Wall -Wno-deprecated-declarations -msse -msse2 -mfpmath=sse -ffast-math -fomit-frame-pointer -fno-finite-math-only -std=c++11 -pthread -L./ 
EXTERNALS = jack libxml++-2.6 liblo sndfile

$(GUIOBJECTS): EXTERNALS += gtkmm-3.0

tascar_hoadisplay: EXTERNALS += $(GTKEXT)

tascar_hdspmixer: EXTERNALS += alsa

tascar_gui tascar_pdf: LDLIBS += -ltascargui `pkg-config --libs $(EXTERNALS)`
tascar_gui tascar_pdf: EXTERNALS += $(GTKEXT)

LDLIBS += `pkg-config --libs $(EXTERNALS)`
CXXFLAGS += `pkg-config --cflags $(EXTERNALS)`

LDLIBS += -ldl

all: lib bins

bins:
	mkdir -p build
	test "$(DEFFULLVERSION)" = "`cat build/tascarver.h`" || echo "$(DEFFULLVERSION)" > build/tascarver.h
	$(MAKE) -C build -f ../Makefile $(BINFILES)

lib:
	mkdir -p build
	test "$(DEFFULLVERSION)" = "`cat build/tascarver.h`" || echo "$(DEFFULLVERSION)" > build/tascarver.h
	$(MAKE) -C build -f ../Makefile libtascar.a libtascargui.a $(RECEIVERMODS) $(SOURCEMODS) $(TASCARMODDLLS) $(AUDIOPLUGINDLLS)

libtascar.a: $(OBJECTS)
	ar rcs $@ $^

libtascargui.a: $(GUIOBJECTS)
	ar rcs $@ $^

install:
	$(MAKE) -C build -f ../Makefile $(INSTBIN)

uninstall:
	rm -f $(INSTBIN)

clean:
	rm -Rf *~ src/*~ build doc/html

VPATH = ../src
# ../src/hoafilt

.PHONY : doc test

test:
	$(MAKE) -C test

doc:
	cd doc && sed -e 's/PROJECT.NUMBER.*=.*/&'`cat ../version`'/1' doxygen.cfg > .temp.cfg && doxygen .temp.cfg
	rm -Rf doc/.temp.cfg

include $(wildcard *.mk)

tascar_gui: libtascargui.a

$(BINFILES) $(RECEIVERMODS) $(SOURCEMODS) $(TASCARMODDLLS) $(AUDIOPLUGINDLLS): libtascar.a

tascar_pdf: libtascargui.a

$(PREFIX)/bin/%: %
	cp $< $@

$(PREFIX)/lib/%: %
	cp $< $@

%: %.o
	$(CXX) $(CXXFLAGS) $(LDLIBS) $^ -o $@

%.o: %.cc
	$(CPP) $(CPPFLAGS) -MM -MF $(@:.o=.mk) $<
	$(CXX) $(CXXFLAGS) -c $< -o $@

#dist: clean doc
dist: clean
	$(MAKE) -C doc
	$(MAKE) DISTNAME=tascar-`cat version` bz2

disttest:
	$(MAKE) DISTNAME=tascar-`cat version` disttest2

disttest2:
	rm -Rf $(DISTNAME) && tar xjf $(DISTNAME).tar.bz2
	cd $(DISTNAME) && $(MAKE)

bz2:
	rm -Rf $(DISTNAME) $(DISTNAME).files
	(cat files; find doc/html -print) | sed 's/.*/$(DISTNAME)\/&/g' > $(DISTNAME).files
	ln -s . $(DISTNAME)
	tar cjf $(DISTNAME).tar.bz2 --no-recursion -T $(DISTNAME).files
	rm -Rf $(DISTNAME) $(DISTNAME).files


tascar_ambdecoder: LDLIBS += `pkg-config --libs gsl`

tascarreceiver_hoa2d.so: LDLIBS+=-lfftw3f
tascarreceiver_hoa2d_dw.so: LDLIBS+=-lfftw3f
tascar_ap_lipsync.so: LDLIBS+=-lfftw3f
tascar_ap_lipsync_paper.so: LDLIBS+=-lfftw3f
tascar_hoafdnrot.so: LDLIBS+=-lfftw3f

tascar_lsljacktime.so tascar_lslsl tascar_lsljacktime tascar_levels2osc.so tascar_lslactor.so: LDLIBS+=-llsl

tascarreceiver_%.so: receivermod_%.cc
	$(CXX) -shared -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

tascarsource_%.so: tascarsource_%.cc
	$(CXX) -shared -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

tascar_%.so: tascarmod_%.cc
	$(CXX) -shared -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

tascar_ap_%.so: tascar_ap_%.cc
	$(CXX) -shared -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

tascar_ltcgen.so: EXTERNALS+=ltc

fullversion:
	echo $(FULLVERSION) >$@

# Local Variables:
# compile-command: "make"
# End:
