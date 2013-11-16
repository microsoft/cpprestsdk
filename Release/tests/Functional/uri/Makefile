CXXFLAGS=$(BASE_CXXFLAGS) -I$(CASABLANCA_INCLUDE_DIR) -I$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/ -include "$(CASABLANCA_INCLUDE_DIR)/../src/pch/stdafx.h"
LDFLAGS=-L"$(OUTPUT_DIR)" $(BASE_LDFLAGS)
LDLIBS=-lcasablanca -lunittestpp -l$(BOOST_SYSTEM) -l$(BOOST_THREAD) -lpthread

OUTPUT_DIR?=./$(MODE)

ifeq ($(UNAME),LINUX)
PKGCONFIG_CFLAGS = $(shell pkg-config libxml++-2.6 --cflags)
endif

$(OUTPUT_DIR)/uri_test.so: accessor_tests.cpp combining_tests.cpp constructor_tests.cpp conversions_tests.cpp diagnostic_tests.cpp encoding_tests.cpp operator_tests.cpp splitting_tests.cpp uri_builder_tests.cpp
	$(CXX) -fPIC -shared $(CXXFLAGS) $(PKGCONFIG_CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@ 
