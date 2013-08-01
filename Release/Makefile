SUBDIRS = src tests samples

bad:
	@echo "Please 'make' one of: release, debug, all, or clean."

#CXX=clang
#CXX=/usr/lib/gcc-snapshot/bin/g++
CXX=g++-4.8
export CXX

mk_out_dir:
	mkdir -p $(OUTPUT_DIR)

release: OPTIMIZATION_LEVEL = -O2
release: MODE = Release$(shell getconf LONG_BIT)
release: OUTPUT_DIR = $(PWD)/../Binaries/$(MODE)
release: mk_out_dir tests samples

debug: OPTIMIZATION_LEVEL = -O0 -g -gdwarf-2
debug: MODE = Debug$(shell getconf LONG_BIT)
debug: OUTPUT_DIR = $(PWD)/../Binaries/$(MODE)
debug: mk_out_dir tests samples

clean: RMODE = Release$(shell getconf LONG_BIT)
clean: DMODE = Debug$(shell getconf LONG_BIT)
clean: ROUTPUT_DIR = $(PWD)/../Binaries/$(RMODE)
clean: DOUTPUT_DIR = $(PWD)/../Binaries/$(DMODE)

export MODE

WARNINGS=-Wall -Wextra -Wunused-parameter -Wcast-align -Wcast-qual -Wconversion -Wformat=2 -Winit-self -Winvalid-pch -Wmissing-format-attribute -Wmissing-include-dirs -Wpacked -Wredundant-decls -Wunreachable-code
SUPPRESSIONS=-Wno-deprecated -Wno-unknown-pragmas -Wno-reorder -Wno-unused-function -Wno-char-subscripts -Wno-switch -Wno-unused-but-set-parameter -Wno-deprecated -Wno-unused-value -Wno-unused-local-typedefs

# C++11 excludes GCC < 4.7, which is desired anyway
OTHER_OPTIONS=-std=c++11 -fno-strict-aliasing

STRICT_BASE_CXXFLAGS=$(OPTIMIZATION_LEVEL) $(OTHER_OPTIONS) $(WARNINGS) $(SUPPRESSIONS) -Werror -pedantic
export STRICT_BASE_CXXFLAGS

BASE_CXXFLAGS=$(OPTIMIZATION_LEVEL) $(OTHER_OPTIONS) $(WARNINGS) $(SUPPRESSIONS) -Werror
export BASE_CXXFLAGS

LAX_BASE_CXXFLAGS=$(OPTIMIZATION_LEVEL) $(SUPPRESSIONS) $(OTHER_OPTIONS) -fpermissive
export LAX_BASE_CXXFLAGS

BASE_LDFLAGS=-Wl,-z,defs 
export BASE_LDFLAGS

export OUTPUT_DIR

CASABLANCA_INCLUDE_DIR=$(PWD)/include/
export CASABLANCA_INCLUDE_DIR

CASABLANCA_DIR=$(PWD)/
export CASABLANCA_DIR

CASABLANCA_TESTS_DIR=$(PWD)/tests
export CASABLANCA_TESTS_DIR

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
	@find ../Binaries -iname *.so -exec rm '{}' \;
	@find ../Binaries -iname *.txt -exec rm '{}' \;
	@find ../Binaries -iname SearchFile -exec rm '{}' \;
	@find ../Binaries -iname BingRequest -exec rm '{}' \;
	@find ../Binaries -iname syncdir -exec rm '{}' \;
	@find ../Binaries -iname test_runner -exec rm '{}' \;
