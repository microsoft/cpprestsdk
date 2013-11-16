CXXFLAGS=$(LAX_BASE_CXXFLAGS) -I. -I$(CASABLANCA_INCLUDE_DIR) -I$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/ -I$(CASABLANCA_TESTS_DIR)/Common/utilities/include -Iinclude

ifeq ($(UNAME),APPLE)
	CXXFLAGS += -include compat/apple_compat.h
else
	CXXFLAGS += -include compat/linux_compat.h
endif

LDFLAGS=-L"$(OUTPUT_DIR)" $(BASE_LDFLAGS)
LDLIBS=-lcasablanca -lunittestpp -l$(BOOST_SYSTEM) -l$(BOOST_THREAD) -l$(BOOST_LOCALE) -lpthread -lstdc++

OUTPUT_DIR?=./$(MODE)

FILENAME = $(OUTPUT_DIR)/libhttptest_utilities

-include $(FILENAME).d

ifeq ($(UNAME),LINUX)
PKGCONFIG_CFLAGS = $(shell pkg-config libxml++-2.6 --cflags)
endif

$(FILENAME).so: \
	http_asserts.cpp \
	test_http_client.cpp \
	test_http_server.cpp \
	test_server_utilities.cpp
	$(CXX) $(CXXFLAGS) -fPIC -shared $(PKGCONFIG_CFLAGS) $(LDFLAGS) -MMD -MF $(FILENAME).d $(filter %.cpp, $^) $(LDLIBS) -o $@