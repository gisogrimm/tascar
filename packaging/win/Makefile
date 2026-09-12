include ../../config.mk

ALL_CSV=$(wildcard *.csv)

PACKVERSION=$(FULLVERSION)-windows
ifeq "$(DEBUG)" "yes"
PACKVERSION:=$(PACKVERSION)-debug
endif
CYGCHECK=cygcheck
MSYSTEM=ucrt64
ifeq "x$(ARCH)" "xarm64"
  CYGCHECK:=../cygcheck_for_arm64.sh
  MSYSTEM:=clangarm64
endif

all:
	rm -Rf tascar;\
	mkdir -p tascar/{scripts,share/icons,share/glib-2.0,lib};\
	rm -f tascar-$(PACKVERSION).zip;\
	cp -v ../../*/build/[lt]*.{dll,exe} tascar/;\
	cp -r ../../scripts/*.m tascar/scripts/;\
	cp -r /$(MSYSTEM)/share/icons/Adwaita tascar/share/icons/;\
	cp -r /$(MSYSTEM)/share/icons/hicolor tascar/share/icons/;\
	cp -r /$(MSYSTEM)/share/glib-2.0/schemas tascar/share/glib-2.0/;\
	cp -r /$(MSYSTEM)/lib/gdk-pixbuf-2.0 tascar/lib/;\
	cp /$(MSYSTEM)/bin/gdbus.exe tascar/;\
	cp /$(MSYSTEM)/bin/broadwayd.exe tascar/;\
	cp /$(MSYSTEM)/bin/librsvg-2-2.dll tascar/;\
	(cd tascar; cp -v $$($(CYGCHECK) ./* ./lib/gdk-pixbuf-2.0/*/*.dll |  egrep "ucrt64|clangarm64" | sort -bu) .;mkdir -p share/{icons,glib-2.0} lib); \
	cp ../../artwork/*.png tascar/;\
	mkdir -p "tascar/examples/sounds";\
	cp -r "../../manual/examples/"example_*.tsc "tascar/examples";\
	cp -r "../../manual/examples/"*.wav* "tascar/examples";\
	cp -r "../../manual/examples/sounds/"*.wav* "tascar/examples/sounds";\
	(cd tascar; glib-compile-schemas share/glib-2.0/schemas);\
	zip -r tascar-$(PACKVERSION).zip tascar/;

#\
#	(cd tascar; find /d/ -name "Adwaita*"; find /d/ -name "glib-2.0"; find /d/ -name "hicolor"; find /d/ -name "librsvg-2-2.dll";echo "---";echo "---";ls /ucrt64/;echo "packaging completed.";)

ver:
	@echo "Packaging: $(PACKVERSION)"
