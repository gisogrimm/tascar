all: build

BUILD_DIR = build

include ../config.mk
include ../rules.mk

clean:
	rm -Rf googlemock googletest $(BUILD_DIR)
	git clean -xfd

googlemock: $(BUILD_DIR)/googlemock.is_installed

$(BUILD_DIR)/googlemock.is_installed: googletest/WORKSPACE \
	$(BUILD_DIR)/lib/.directory $(BUILD_DIR)/include/.directory
	echo $(CXXFLAGS)
	cd googletest && cmake -G "Unix Makefiles" . && $(MAKE)
	cp -a googletest/googletest/include/gtest $(BUILD_DIR)/include/
	cp -a googletest/googlemock/include/gmock $(BUILD_DIR)/include/
	touch $@

googletest/WORKSPACE:
	git clone https://github.com/google/googletest
	(cd googletest && git checkout v1.16.0)

ifeq ($(OS),Windows_NT)

libmysofa:

else

libmysofa:
	(mkdir -p build && cd libmysofa-1.3.2/build && cmake .. && $(MAKE))

endif

ifeq ($(UNAME_S),Linux)

CMAKEVERSION := $(shell cmake --version|head -1|sed -e 's/.*version //')
CMAKE_V_MAJ :=$(shell echo $(CMAKEVERSION)|cut -f1 -d.)
CMAKE_V_MIN :=$(shell echo $(CMAKEVERSION)|cut -f2 -d.)

CMAKE_GT_3_12 := $(shell test $(CMAKE_V_MAJ) -gt 3 -o \( $(CMAKE_V_MAJ) -eq 3 -a $(CMAKE_V_MIN) -ge 12 \) && echo "YES")

ifeq ($(CMAKE_GT_3_12),YES)

liblsl:
	(echo $(CMAKEVERSION) OK? $(CMAKE_GT_3_12); cd liblsl-1.16.2 && cmake -S ./ -B build/ -DCMAKE_INSTALL_PREFIX="../build" -DCMAKE_CXX_FLAGS="-fPIC" -DLSL_BUILD_STATIC=true && cmake --build build -j 2 --config Release --target install)

else

liblsl:
	echo "cmake is too old ($(CMAKEVERSION)), let's hope that liblsl is installed from other sources."

endif

else

ifeq ($(OS),Windows_NT)
liblsl: liblsl-1.16.2-Win_amd64.zip
	(mkdir -p build && cd build && unzip -o ../$< || true)
else

liblsl:
endif

endif
