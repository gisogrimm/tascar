# rules:

ifeq ($(USE_SYSTEM_LIBS),1)
CXXFLAGS += $(GCCCOVFLAGS)
LDLIBS += $(COVLIBS)
else
CXXFLAGS += -I../external_libs/$(BUILD_DIR)/include $(GCCCOVFLAGS)
LDLIBS += -L../external_libs/$(BUILD_DIR)/lib64 -L../external_libs/$(BUILD_DIR)/lib $(COVLIBS)
endif
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

LSLCFLAGS =
LSLLIBS = -llsl

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
BREWPREFIX := $(shell brew --prefix)
LIBTASCARDLL=../libtascar/$(BUILD_DIR)/libtascar.dylib
CXXFLAGS += -I$(BREWPREFIX)/include -DISMACOS
CPPFLAGS += -I$(BREWPREFIX)/include
LDFLAGS += -L$(BREWPREFIX)/lib
LSLFLAGSFIND := $(addprefix -I,$(dir $(shell find $(BREWPREFIX)/ -name lsl_cpp.h)))
CXXFLAGS += $(LSLFLAGSFIND)
#  LDFLAGS += -F$(BREWPREFIX)/Cellar/lsl/1.17.4/Frameworks -framework lsl
#  $(addprefix -I,$(dir $(shell find $(BREWPREFIX)/ -name lsl_cpp.h)))
# -framework Lsl
LSLCFLAGS = -I$(BREWPREFIX)/Frameworks/lsl.framework/Headers
LSLLIBS = -F$(BREWPREFIX)/Frameworks/ -framework lsl
CXXFLAGS += $(LSLCFLAGS)
endif
endif

ifeq ($(USE_SYSTEM_LIBS),1)
GTEST_LIBPATH =
GTEST_LIBS = -lgmock_main -lgmock -lgtest -lpthread
else
GTEST_LIBPATH = -L../external_libs/googletest/lib/
GTEST_LIBS = -lgmock_main -lgmock -lgtest_main -lgtest -lpthread
endif

$(BUILD_DIR)/unit-test-runner: $(BUILD_DIR)/.directory $(unit_tests_test_files) $(patsubst %_unit_tests.cpp, %.cpp , $(unit_tests_test_files))
	if test -n "$(unit_tests_test_files)"; then $(CXX) $(CXXFLAGS) --coverage -o $@ $(wordlist 2, $(words $^), $^) $(LDFLAGS) $(LIBTASCARDLL) $(LDLIBS) $(GTEST_LIBPATH) $(GTEST_LIBS); fi
