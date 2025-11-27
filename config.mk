# variables:
VERSION=0.235.2

ARCH:=$(shell uname -m)

CXXFLAGS = -Wall -Wextra -Wdeprecated-declarations -Wno-psabi -std=c++17 -pthread	\
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

CPPFLAGS = -std=c++17
PREFIX = /usr/local

GITMODIFIED:=$(shell test -z "`git status --porcelain -uno`" || echo "-modified")
COMMITHASH:=$(shell git log -1 --abbrev=7 --pretty='format:%h')
LATEST_RELEASETAG:=$(shell git tag -l "release*" |tail -1)
COMMIT_SINCE_RELEASE:=$(shell git rev-list --count $(LATEST_RELEASETAG)..)

FULLVERSION=$(VERSION).$(COMMIT_SINCE_RELEASE)-$(COMMITHASH)$(GITMODIFIED)

mkfile_name := $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_path := $(subst $(notdir $(mkfile_name)),,$(mkfile_name))

#HAS_LSL:=$(shell $(mkfile_path)/check_for_lsl)
HAS_LSL=yes

HAS_OPENMHA:=$(shell $(mkfile_path)/check_for_openmha)

HAS_OPENCV2:=$(shell $(mkfile_path)/check_for_opencv2)

HAS_OPENCV4:=$(shell $(mkfile_path)/check_for_opencv4)

HAS_WEBKIT:=$(shell $(mkfile_path)/check_for_webkit)

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
