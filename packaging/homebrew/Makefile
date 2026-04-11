.PHONY: install build

#tascar project directory
REPO=../..

include $(REPO)/config.mk

install: usr-local-bin \
         usr-local-lib \
         matlab \
         examples \
         devfiles

build:
	rm -rf homebrew-build-dir
	mkdir homebrew-build-dir
	$(MAKE) -C "$(REPO)" "DESTDIR=$$(realpath homebrew-build-dir)" install
	chmod 755 "homebrew-build-dir/$(PREFIX)/bin/"*
	chmod 755 "homebrew-build-dir/$(PREFIX)/lib/"*

usr-local-bin: build
	mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	rm -f "homebrew-build-dir/$(PREFIX)/bin/"*.mk
	cp -r "homebrew-build-dir/$(PREFIX)/bin/"* "$(DESTDIR)$(PREFIX)/bin"
	cp "$(REPO)/packaging/homebrew/tascar_homebrew_postinstall" "$(DESTDIR)$(PREFIX)/bin"

usr-local-lib: build
	mkdir -p "$(DESTDIR)$(PREFIX)/lib"
	mkdir -p "$(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps/"
	cp -r "homebrew-build-dir/$(PREFIX)/lib/"* "$(DESTDIR)$(PREFIX)/lib"
	cp "$(REPO)/artwork/tascarpro.png" "$(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps/"
	cp "$(REPO)/artwork/tascarspkcalib.png" "$(DESTDIR)$(PREFIX)/share/icons/hicolor/48x48/apps/"

examples: build
	mkdir -p "$(DESTDIR)$(PREFIX)/share/tascar/examples/sounds"
	mkdir -p "$(DESTDIR)$(PREFIX)/share/tascar/examples/decoder"
	cp -r "$(REPO)/manual/examples/"example_*.tsc "$(DESTDIR)$(PREFIX)/share/tascar/examples"
	cp -r "$(REPO)/manual/examples/"*.wav* "$(DESTDIR)$(PREFIX)/share/tascar/examples"
	cp -r "$(REPO)/manual/examples/sounds/"*.wav* "$(DESTDIR)$(PREFIX)/share/tascar/examples/sounds"

matlab: build
	mkdir -p "$(DESTDIR)$(PREFIX)/share/tascar/matlab"
	cp "$(REPO)/scripts/"*.m "$(DESTDIR)$(PREFIX)/share/tascar/matlab"
	cp "$(REPO)/scripts/"netutil* "$(DESTDIR)$(PREFIX)/share/tascar/matlab"

devfiles: build
	mkdir -p "$(DESTDIR)$(PREFIX)/share/tascar"
	mkdir -p "$(DESTDIR)$(PREFIX)/share/tascar/examples/plugins/src"
	mkdir -p "$(DESTDIR)$(PREFIX)/include/tascar"
	mkdir -p "$(DESTDIR)$(PREFIX)/share/doc/libtascar/html/"
	mkdir -p "$(DESTDIR)$(PREFIX)/share/doc/libtascar/html/search/"
	cp "$(REPO)/LICENSE" "$(DESTDIR)$(PREFIX)/share/doc/libtascar"
	cp "$(REPO)/LICENSE" "$(DESTDIR)$(PREFIX)/share/tascar/examples/plugins/"
	cp "$(REPO)/check_for_lsl" "$(DESTDIR)$(PREFIX)/share/tascar/"
	cp "$(REPO)/check_for_opencv2" "$(DESTDIR)$(PREFIX)/share/tascar/"
	cp "$(REPO)/check_for_openmha" "$(DESTDIR)$(PREFIX)/share/tascar/"
	cp "$(REPO)/check_for_webkit" "$(DESTDIR)$(PREFIX)/share/tascar/"
	cp "$(REPO)/config.mk" "$(DESTDIR)$(PREFIX)/share/tascar/"
	cp "$(REPO)/devkit/Makefile" "$(DESTDIR)$(PREFIX)/share/tascar/examples/plugins"
	cp "$(REPO)/devkit/README.md" "$(DESTDIR)$(PREFIX)/share/tascar/examples/plugins/"
	cp "$(REPO)/devkit/src/"*.cc "$(DESTDIR)$(PREFIX)/share/tascar/examples/plugins/src"
	cp "$(REPO)/devkit/test/"*.tsc "$(DESTDIR)$(PREFIX)/share/tascar/examples/plugins"
	cp "$(REPO)/libtascar/build/tascarver.h" "$(DESTDIR)$(PREFIX)/include/tascar/"
	cp "$(REPO)/libtascar/include/"*.h "$(DESTDIR)$(PREFIX)/include/tascar/"
	cp "$(REPO)/rules.mk" "$(DESTDIR)$(PREFIX)/share/tascar/"

clean:
	@rm -rf homebrew-build-dir
