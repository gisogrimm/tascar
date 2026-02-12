# rules:

CXXFLAGS += -I../external_libs/$(BUILD_DIR)/include $(GCCCOVFLAGS)
LDLIBS += -L../external_libs/$(BUILD_DIR)/lib64 -L../external_libs/$(BUILD_DIR)/lib $(COVLIBS)
LDFLAGS += $(LDCOVFLAGS)

build: build/.directory

%/.directory:
	mkdir -p $*
	touch $@

unit-tests: execute-unit-tests $(patsubst %,%-subdir-unit-tests,$(SUBDIRS))

$(patsubst %,%-subdir-unit-tests,$(SUBDIRS)):
	$(MAKE) -C $(@:-subdir-unit-tests=) unit-tests

execute-unit-tests: $(BUILD_DIR)/unit-test-runner
	pwd;if [ -x $< ]; then LD_LIBRARY_PATH=./build:../libtascar/build DYLD_LIBRARY_PATH=./build:../libtascar/build $<; fi

unit_tests_test_files = $(wildcard $(SOURCE_DIR)/*_unit_tests.cc)

$(BUILD_DIR)/unit-test-runner: CXXFLAGS += -I../libtascar/src -I../libtascar/build
$(BUILD_DIR)/unit-test-runner: CPPFLAGS += -I../libtascar/src -I../libtascar/build
$(BUILD_DIR)/unit-test-runner: LDFLAGS += -L../libtascar/build 

ifeq ($(OS),Windows_NT)
  CXXFLAGS += -DISWINDOWS
  LIBTASCARDLL=../libtascar/$(BUILD_DIR)/libtascar.dll
else
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
  LIBTASCARDLL=../libtascar/$(BUILD_DIR)/libtascar.so
  CXXFLAGS += -DISLINUX
  BINFILES += tascar_hdspmixer
endif
ifeq ($(UNAME_S),Darwin)
  LIBTASCARDLL=../libtascar/$(BUILD_DIR)/libtascar.dylib
  CXXFLAGS += -I/opt/homebrew/include -DISMACOS
  CPPFLAGS += -I/opt/homebrew/include
  LDFLAGS += -L/opt/homebrew/lib
  CXXFLAGS += -framework lsl
endif
endif

$(BUILD_DIR)/unit-test-runner: $(BUILD_DIR)/.directory $(unit_tests_test_files) $(patsubst %_unit_tests.cpp, %.cpp , $(unit_tests_test_files))
	if test -n "$(unit_tests_test_files)"; then $(CXX) $(CXXFLAGS) --coverage -o $@ $(wordlist 2, $(words $^), $^) $(LDFLAGS) $(LIBTASCARDLL) $(LDLIBS) -L../external_libs/googletest/lib/ -lgmock_main -lgmock -lgtest_main -lgtest -lpthread; fi

