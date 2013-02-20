CXXFLAGS=$(LAX_BASE_CXXFLAGS) -I. -I$(CASABLANCA_INCLUDE_DIR) -I$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/ -I$(CASABLANCA_TESTS_DIR)/Common/utilities/include
LDFLAGS=-L$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/lib/linux_gcc/ -L"$(OUTPUT_DIR)" $(BASE_LDFLAGS)
LDLIBS=-lcasablanca -lunittestpp -lboost_system -lboost_thread -lcommon_utilities -lpthread -lstdc++ -lm


OUTPUT_DIR?=./$(MODE)

$(OUTPUT_DIR)/asyncop_test.so: fstreambuf_tests.cpp istream_tests.cpp memstream_tests.cpp ostream_tests.cpp stdstream_tests.cpp 
	$(CXX) -fPIC -shared $(CXXFLAGS) $(shell pkg-config libxml++-2.6 --cflags) $(LDFLAGS) $^ $(LDLIBS) -o $@
