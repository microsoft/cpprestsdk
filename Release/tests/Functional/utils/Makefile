CXXFLAGS=$(BASE_CXXFLAGS) -I$(CASABLANCA_INCLUDE_DIR) -I$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/ -include "$(CASABLANCA_INCLUDE_DIR)/../src/pch/stdafx.h"
LDFLAGS=-L$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/lib/linux_gcc/ -L"$(OUTPUT_DIR)" $(BASE_LDFLAGS)
LDLIBS=-lcasablanca -lunittestpp -lboost_system -lboost_thread -lpthread

OUTPUT_DIR?=./$(MODE)

$(OUTPUT_DIR)/utils_test.so: datetime.cpp base64.cpp
	$(CXX) -fPIC -shared $(CXXFLAGS) $(shell pkg-config libxml++-2.6 --cflags) $(LDFLAGS) $^ $(LDLIBS) -o $@ 
