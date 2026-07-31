# variables:
VERSION=0.238.1

# Set to 1 to build against system-provided libmysofa / gtest / gmock
# instead of the copies bundled under external_libs/. See the top-level
# Makefile for the full description.
USE_SYSTEM_LIBS ?= 0
ifeq ($(USE_SYSTEM_LIBS),1)
  USE_BUNDLED_LIBMYSOFA = DO_NOT_USE
  USE_BUNDLED_LIBLSL = DO_NOT_USE
endif

ARCH:=$(shell uname -m)

# On Windows for ARM, msys2 uname -m wrongly reports x86_64, but uname -s
# reports something like MINGW64_NT-10.0-26200-ARM64. Override ARCH to
# arm64 when the output of uname -s ends with ARM64.
ifeq "x$(ARCH)" "xx86_64"
  ifneq (,$(findstring ARM64,$(shell uname -s)))
    ARCH:=arm64
  endif
endif

CXXFLAGS += -Wall -Wextra -Wdeprecated-declarations -Wno-psabi -std=c++17 -pthread	\
-ggdb -fno-finite-math-only -Wno-psabi
# -Wconversion
# -Werror

ifeq ($(OS),Windows_NT)
  DLLEXT=.dll
else
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
  DLLEXT=.so
  EXTERNALS +=  alsa
  CXXFLAGS += -fext-numeric-literals
else
  DLLEXT=.dylib
endif
endif


ifeq "$(ARCH)" "x86_64"
  CXXFLAGS += -msse -msse2 -mfpmath=sse
  ifneq "$(UNAME_S)" "Darwin"
    CXXFLAGS += -ffast-math
  endif
endif

CPPFLAGS += -std=c++17
PREFIX ?= /usr/local

# Development builds derive FULLVERSION from Git metadata so local builds
# include the commit count, commit hash and dirty state. Release/package builds
# from source tarballs may not have a .git directory so allow callers to disable
# this and use the upstream VERSION directly.
DERIVE_VERSION_FROM_GIT ?= 1
ifeq ($(DERIVE_VERSION_FROM_GIT),1)
  # If the source tree contains modifications, append "-modified" to the version string.
  # But ignore alleged symlink modifications detected on Github Windows runners.
  GITMODIFIED:=$(shell test -z "`git status --porcelain -uno | grep -v T.external_libs/libmysofa-1.3.2/share/default.sofa | grep -v D.libtascar/tascar`" || echo "-modified")
  COMMITHASH:=$(shell git log -1 --abbrev=7 --pretty='format:%h')
  LATEST_RELEASETAG:=$(shell git tag -l "release*" |tail -1)
  COMMIT_SINCE_RELEASE:=$(shell git rev-list --count $(LATEST_RELEASETAG)..)
  FULLVERSION=$(VERSION).$(COMMIT_SINCE_RELEASE)-$(COMMITHASH)$(GITMODIFIED)
else
  FULLVERSION := $(VERSION)
endif

# Bake the plugin search path into libtascar so dlopen() finds plugins
# installed under $(LIBDIR)/$(PLUGIN_LIB_SUBDIR)/ at runtime. See the top-level
# Makefile for PLUGIN_LIB_SUBDIR.
ifneq ($(strip $(PLUGIN_LIB_SUBDIR)),)
  CXXFLAGS += -DTASCAR_DEFAULT_PLUGINDIR='"$(LIBDIR)/$(PLUGIN_LIB_SUBDIR)/"'
endif

mkfile_name := $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_path := $(subst $(notdir $(mkfile_name)),,$(mkfile_name))

#HAS_LSL:=$(shell $(mkfile_path)/check_for_lsl)
HAS_LSL=yes
HAS_OPENMHA:=$(shell $(mkfile_path)/check_for_openmha)
HAS_OPENCV2:=$(shell $(mkfile_path)/check_for_opencv2)
HAS_OPENCV4:=$(shell $(mkfile_path)/check_for_opencv4)
HAS_WEBKIT:=$(shell $(mkfile_path)/check_for_webkit)
HAS_MOSQUITTO:=$(shell pkg-config libmosquitto && echo "yes" || echo "no")
HAS_NLOHMANN:=$(shell $(mkfile_path)/check_for_nlohmann)

BUILD_DIR = build
SOURCE_DIR = src

export VERSION
export SOURCE_DIR
export BUILD_DIR
export CXXFLAGS
export HAS_LSL
export HAS_OPENMHA
export HAS_OPENCV2
export HAS_WEBKIT
export HAS_MOSQUITTO
export HAS_NLOHMANN
