all: build bin

include ../config.mk
include ../rules.mk

TASCARVERSION=$(FULLVERSION)
TASCARVERSION_MAJOR = $(shell echo $(TASCARVERSION)|sed -e 's/\..*//1')
TASCARVERSION_MINOR = $(shell echo $(TASCARVERSION)|sed -e 's/[^\.]*\.//1' -e 's/\..*//1')

CXXFLAGS += -DTASCARVERSION_MAJOR=$(TASCARVERSION_MAJOR) -DTASCARVERSION_MINOR=$(TASCARVERSION_MINOR) -DTASCARVERSION=\"$(TASCARVERSION)\"

BINFILES = build/tascar build/tascar_spkcalib
OBJECTS = build/scene_manager.o build/tascar_mainwindow.o

ifeq "$(DEBUG)" "yes"
CXXFLAGS += -Wall -g -L./
else
CXXFLAGS += -Wall -O3 -L./
endif

EXTERNALS = jack liblo sndfile gtkmm-3.0 glibmm-2.4 gtksourceviewmm-3.0 xerces-c

ifneq ($(OS),Windows_NT) # There is no Gtk Webkit for Windows
HAS_WEBKIT40 = $(shell pkg-config webkit2gtk-4.0 && echo "yes"||echo "no")
HAS_WEBKIT30 = $(shell pkg-config webkit2gtk-3.0 && echo "yes"||echo "no")
ifeq "$(HAS_WEBKIT40)" "yes"
EXTERNALS += webkit2gtk-4.0
CXXFLAGS += -DWEBKIT2GTK40=1
else
ifeq "$(HAS_WEBKIT30)" "yes"
EXTERNALS += webkit2gtk-3.0
CXXFLAGS += -DWEBKIT2GTK30=1
endif
endif
endif

LDLIBS += -ltascar -ltascargui -ldl -lfftw3f
LDFLAGS += -L../libtascar/build
CXXFLAGS += -I../libtascar/include -I./src -I../libtascar/build -I./build
CPPFLAGS += -I../libtascar/include -I./src -I../libtascar/build -I./build

LDLIBS += `pkg-config --libs $(EXTERNALS)`
CXXFLAGS += `pkg-config --cflags $(EXTERNALS)`

bin: $(BINFILES)

install:
	$(MAKE) -C build -f ../Makefile $(INSTBIN)

uninstall:
	rm -f $(INSTBIN)

clean:
	rm -Rf *~ src/*~ build doc/html

tascarclean:
	$(MAKE) -C ../ clean

$(PREFIX)/bin/%: %
	cp $< $@

build/tascar: $(OBJECTS)
build/tascar: TSCOBJECTS=$(OBJECTS)

build/%: build/%.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< $(TSCOBJECTS) $(LDLIBS) -o $@

build/%.o: src/%.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

ifeq ($(OS),Windows_NT)
ICON_PATH_REPLACEMENT = -e 's|usr/share/icons/hicolor/48x48/apps/|./|'
LDFLAGS += -mwindows
else
ICON_PATH_REPLACEMENT = -e 's/>usr/>\/usr/1'
endif

build/%_glade.h: src/%.glade build/.directory
	cat $< | sed -e 's/tascarversion/$(TASCARVERSION)/g' -e '/name="authors"/ r ../contributors' $(ICON_PATH_REPLACEMENT) >$*_glade
	echo "#include <string>" > $@
	xxd -i $*_glade >> $@
	echo "std::string ui_"$*"((const char*)"$*"_glade,(size_t)"$*"_glade_len);" >> $@
	rm -f $*_glade

ALLSVG := $(wildcard src/*.svg)

build/tascar.res.c: $(ALLSVG)

build/%.res.c: src/%.res
	(cd src && glib-compile-resources --generate-source --target=../$@ ../$<)

build/tascar_mainwindow.o: build/tascar.res.c
build/tascar: build/tascar_glade.h

build/tascar_spkcalib.o: build/spkcalib_glade.h

build/tascar_mainwindow.o: src/tascar_mainwindow.h

build/tascar build/tascar_mainwindow.o: CXXFLAGS += -Wno-deprecated-declarations

#build/tascar_%.xpm:
#	convert -font Helvetica -size 24x24 -gravity center label:$* -transparent white $@
#	sed -i -e 's/static char/static const char/1' $@


# Local Variables:
# compile-command: "make"
# End:
