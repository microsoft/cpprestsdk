CXXFLAGS=$(LAX_BASE_CXXFLAGS) -I$(CASABLANCA_INCLUDE_DIR) -I$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/ -I$(CASABLANCA_TESTS_DIR)/Common/utilities/include   
LDFLAGS=-L$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/lib/linux_gcc/ -L"$(OUTPUT_DIR)" $(LD_BASE_FLAGS)
LDLIBS=-lcasablanca -lunittestpp -lboost_system -lboost_thread -lpthread -lcommon_utilities

OUTPUT_DIR?=./$(MODE)


# unsupported: pplx_agents

$(OUTPUT_DIR)/pplx_test.so: pplx_op_test.cpp pplx_task_options.cpp pplxtask_tests.cpp pplx_scheduler.cpp stdafx.cpp 
	$(CXX) -fPIC -shared $(CXXFLAGS) $(shell pkg-config libxml++-2.6 --cflags) $(LDFLAGS) $^ $(LDLIBS) -o $@


