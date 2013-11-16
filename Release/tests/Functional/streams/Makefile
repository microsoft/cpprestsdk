CXXFLAGS=$(LAX_BASE_CXXFLAGS) -I. -I$(CASABLANCA_INCLUDE_DIR) -I$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/ -I$(CASABLANCA_TESTS_DIR)/Common/utilities/include
LDFLAGS=-L"$(OUTPUT_DIR)" $(BASE_LDFLAGS)
LDLIBS=-lcasablanca -lunittestpp -l$(BOOST_SYSTEM) -l$(BOOST_THREAD) -lcommon_utilities -lpthread -lstdc++ -lm


OUTPUT_DIR?=./$(MODE)

ifeq ($(UNAME),LINUX)
PKGCONFIG_CFLAGS = $(shell pkg-config libxml++-2.6 --cflags)
endif

$(OUTPUT_DIR)/asyncop_test.so: fstreambuf_tests.cpp istream_tests.cpp memstream_tests.cpp ostream_tests.cpp stdstream_tests.cpp 
	$(CXX) -fPIC -shared $(CXXFLAGS) $(PKGCONFIG_CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@
