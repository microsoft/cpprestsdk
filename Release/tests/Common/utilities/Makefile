CXXFLAGS=$(LAX_BASE_CXXFLAGS) -I../../../include/ -I../../Common/UnitTestpp/ -L"$(OUTPUT_DIR)" -fPIC -Iinclude

ifeq ($(UNAME),APPLE)
	CXXFLAGS += -include ../../../include/compat/apple_compat.h
else
	CXXFLAGS += -include ../../../include/compat/linux_compat.h
endif

LDLIBS=-lcasablanca -lunittestpp -l$(BOOST_SYSTEM) -l$(BOOST_THREAD) -lpthread -lstdc++

OUTPUT_DIR?=./$(MODE)

ifeq ($(UNAME),LINUX)
PKGCONFIG_CFLAGS = $(shell pkg-config libxml++-2.6 --cflags)
endif

$(OUTPUT_DIR)/libcommon_utilities.so: os_utilities.cpp stdafx.cpp
	$(CXX) $(CXXFLAGS) $(PKGCONFIG_CFLAGS) $(LDFLAGS) -shared $^ $(LDLIBS) -o $@
