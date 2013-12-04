TOP ?= $(shell pwd)

SUBDIRS = src tests samples

bad:
	@echo "Please 'make' one of: release, debug, all, or clean."

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	UNAME=LINUX
	CXX=g++-4.8
endif
ifeq ($(UNAME_S),Darwin)
	UNAME=APPLE
    CXX=clang++	
endif

export UNAME
export CXX

mk_out_dir:
	mkdir -p $(OUTPUT_DIR)

release: OPTIMIZATION_LEVEL = -O2
release: MODE = Release$(shell getconf LONG_BIT)
release: OUTPUT_DIR = $(PWD)/../Binaries/$(MODE)
release: mk_out_dir tests samples

debug: OPTIMIZATION_LEVEL = -O0 -g -ggdb
debug: MODE = Debug$(shell getconf LONG_BIT)
debug: OUTPUT_DIR = $(TOP)/../Binaries/$(MODE)
debug: mk_out_dir tests samples

clean: RMODE = Release$(shell getconf LONG_BIT)
clean: DMODE = Debug$(shell getconf LONG_BIT)
clean: ROUTPUT_DIR = $(TOP)/../Binaries/$(RMODE)
clean: DOUTPUT_DIR = $(TOP)/../Binaries/$(DMODE)

export MODE

WARNINGS=-Wall -Wextra -Wunused-parameter -Wcast-align -Wcast-qual -Wconversion -Wformat=2 -Winit-self -Winvalid-pch -Wmissing-format-attribute -Wmissing-include-dirs -Wpacked -Wredundant-decls -Wunreachable-code

ifeq ($(UNAME),LINUX)
SUPPRESSIONS=-Wno-deprecated -Wno-unknown-pragmas -Wno-reorder -Wno-unused-function -Wno-char-subscripts -Wno-switch -Wno-unused-but-set-parameter -Wno-deprecated -Wno-unused-value -Wno-unused-local-typedefs
# C++11 excludes GCC < 4.7, which is desired anyway
OTHER_OPTIONS=-std=c++11 -fno-strict-aliasing
endif
ifeq ($(UNAME),APPLE)
SUPPRESSIONS=-Wno-overloaded-virtual -Wno-sign-conversion -Wno-deprecated -Wno-unknown-pragmas -Wno-reorder -Wno-unused-function -Wno-char-subscripts -Wno-switch -Wno-unused-parameter -Wno-unused-variable -Wno-deprecated -Wno-unused-value -Wno-unknown-warning-option -Wno-return-type-c-linkage
OTHER_OPTIONS=-stdlib=libc++ -std=c++11 -fno-strict-aliasing
endif

STRICT_BASE_CXXFLAGS=$(OPTIMIZATION_LEVEL) $(OTHER_OPTIONS) $(WARNINGS) $(SUPPRESSIONS) -Werror -pedantic
export STRICT_BASE_CXXFLAGS

BASE_CXXFLAGS=$(OPTIMIZATION_LEVEL) $(OTHER_OPTIONS) $(WARNINGS) $(SUPPRESSIONS) -Werror
export BASE_CXXFLAGS

LAX_BASE_CXXFLAGS=$(OPTIMIZATION_LEVEL) $(SUPPRESSIONS) $(OTHER_OPTIONS) 
export LAX_BASE_CXXFLAGS

#Disallows undefined symbols in object files (not supported on MacOS)
ifeq ($(UNAME),LINUX)
BASE_LDFLAGS=-Wl,-z,defs
endif
export BASE_LDFLAGS

export OUTPUT_DIR

CASABLANCA_INCLUDE_DIR=$(TOP)/include/
export CASABLANCA_INCLUDE_DIR

CASABLANCA_DIR=$(TOP)/
export CASABLANCA_DIR

CASABLANCA_TESTS_DIR=$(TOP)/tests
export CASABLANCA_TESTS_DIR

# boost libraries: use '-mt' suffix for Apple
ifeq ($(UNAME),LINUX)
export BOOST_SYSTEM=boost_system
export BOOST_THREAD=boost_thread
export BOOST_LOCALE=boost_locale
export BOOST_REGEX=boost_regex
export BOOST_FILE_SYSTEM=boost_filesystem
endif
ifeq ($(UNAME),APPLE)
export BOOST_SYSTEM=boost_system-mt
export BOOST_THREAD=boost_thread-mt
export BOOST_LOCALE=boost_locale-mt
export BOOST_REGEX=boost_regex-mt
export BOOST_FILE_SYSTEM=boost_filesystem-mt
endif

tests: src
samples: src

.PHONY: test $(SUBDIRS) all clean release debug mk_out_dir

$(SUBDIRS):
	$(MAKE) -C $@

all:
	make debug
	make release

test: tests

clean:
	@[ -e src ] # sanity check directory
	@find . -iname *.so -exec rm '{}' \;
	@find . -iname *.o -exec rm '{}' \;
	@find . -iname *.d -exec rm '{}' \;
	@find ../Binaries -iname *.so -exec rm '{}' \;
	@find ../Binaries -iname *.txt -exec rm '{}' \;
	@find ../Binaries -iname *.d -exec rm '{}' \;
	@find ../Binaries -iname SearchFile -exec rm '{}' \;
	@find ../Binaries -iname BingRequest -exec rm '{}' \;
	@find ../Binaries -iname syncdir -exec rm '{}' \;
	@find ../Binaries -iname test_runner -exec rm '{}' \;
