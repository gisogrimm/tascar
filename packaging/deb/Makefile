include ../../config.mk

ALL_CSV=$(wildcard *.csv)

CODENAME=$(shell lsb_release -sc)

PACKVERSION=$(FULLVERSION)-gcc-$(shell gcc -dumpversion)-$(shell lsb_release -si)$(shell lsb_release -sr)
ifeq "$(DEBUG)" "yes"
PACKVERSION:=$(PACKVERSION)-debug
endif
LIBVERSION=$(VERSION)

LSLVERSION:=$(shell dpkg-query --show --showformat='$${Version}' liblsl)

all:
	rm -Rf tascar;\
	mkdir -p tascar/$(CODENAME);\
	for c in $(ALL_CSV); do echo "-- $${c} ---------"; $(MAKE) $${c}.version && (cd ../.. && LIBVERSION=$(LIBVERSION) LSLVERSION=$(LSLVERSION) ./packaging/deb/htchdebian-mkdeb packaging/deb/$$c `cat packaging/deb/$${c}.version` ); done;\
	mv ../../*.deb tascar/$(CODENAME);\
	for c in $(ALL_CSV:.csv=); do ls tascar/$(CODENAME)/$${c}_*.deb || return 1; done


ver:
	@echo "Packaging: $(PACKVERSION)"
	@echo "Library: $(LIBVERSION)"
	@echo "LSL: $(LSLVERSION)"

.PHONY: $(wildcard *.version)

%.version:
	echo $(PACKVERSION) > $@

clean:
	rm -Rf tascar
	rm -f *.version *.documented a.out

ALLPLUGINS = $(shell cat tascarcli.csv tascarpro.csv | grep -e 'tascar_.*\.so' -e 'tascarreceiver_.*\.so' | sed -e 's/.*tascar_ap_//1' -e 's/.*tascar_//1' -e 's/.*tascarreceiver_//1' -e 's/\.so.*//1')


%.documented: ../tascar/manual/mod%.tex
	echo "general purpose plugin" > $@

%.documented: ../tascar/manual/apmod%.tex
	echo "audio plugin" > $@

%.documented: ../tascar/manual/actmod%.tex
	echo "actor plugin" > $@

%.documented: ../tascar/manual/recspk%.tex
	echo "speaker based receiver plugin" > $@

%.documented: ../tascar/manual/recgen%.tex
	echo "generic receiver plugin" > $@

%.documented: ../tascar/manual/recrev%.tex
	echo "reverb receiver plugin" > $@

documented: $(patsubst %,%.documented,$(ALLPLUGINS))

test_modified:
	echo $(FULLVERSION) | grep -v -e modified
