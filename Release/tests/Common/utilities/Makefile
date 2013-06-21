CXXFLAGS=$(LAX_BASE_CXXFLAGS) -I../../../include/ -I../../Common/UnitTestpp/ -include ../../../include/compat/linux_compat.h -L../../Common/UnitTestpp/lib/linux_gcc/ -L"$(OUTPUT_DIR)" -fPIC -Iinclude

LDLIBS=-lcasablanca -lunittestpp -lboost_system -lboost_thread -lpthread -lstdc++

OUTPUT_DIR?=./$(MODE)

$(OUTPUT_DIR)/libcommon_utilities.so: os_utilities.cpp stdafx.cpp
	$(CXX) $(CXXFLAGS) $(shell pkg-config libxml++-2.6 --cflags) $(LDFLAGS) -shared $^ $(LDLIBS) -o $@
