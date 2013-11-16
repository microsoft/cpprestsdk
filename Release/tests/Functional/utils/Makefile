CXXFLAGS=$(BASE_CXXFLAGS) -I$(CASABLANCA_INCLUDE_DIR) -I$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/ -include "$(CASABLANCA_INCLUDE_DIR)/../src/pch/stdafx.h"
LDFLAGS=-L"$(OUTPUT_DIR)" $(BASE_LDFLAGS)
LDLIBS=-lcasablanca -lunittestpp -l$(BOOST_SYSTEM) -l$(BOOST_THREAD) -lpthread

OUTPUT_DIR?=./$(MODE)

ifeq ($(UNAME),LINUX)
PKGCONFIG_CFLAGS = $(shell pkg-config libxml++-2.6 --cflags)
endif

$(OUTPUT_DIR)/utils_test.so: datetime.cpp base64.cpp strings.cpp macro_test.cpp
	$(CXX) -fPIC -shared $(CXXFLAGS) $(PKGCONFIG_CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@ 
