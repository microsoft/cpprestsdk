CXXFLAGS=$(BASE_CXXFLAGS) -I"$(CASABLANCA_INCLUDE_DIR)" -I$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/
LDFLAGS=-L$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/lib/linux_gcc/ -L"$(OUTPUT_DIR)" $(BASE_LDFLAGS)
LDLIBS=-lcasablanca -lunittestpp -lboost_system -lboost_thread -lpthread -lstdc++ -lm

OUTPUT_DIR?=./$(MODE)

$(OUTPUT_DIR)/json_test.so: construction_tests.cpp negative_parsing_tests.cpp parsing_tests.cpp to_as_and_operators_tests.cpp
	$(CXX) -fPIC -shared $(CXXFLAGS) $(shell pkg-config libxml++-2.6 --cflags) $(LDFLAGS) $^ $(LDLIBS) -o $@ 


