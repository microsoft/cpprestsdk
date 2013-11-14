CXXFLAGS=$(LAX_BASE_CXXFLAGS) -I$(CASABLANCA_INCLUDE_DIR) -I$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/ -I$(CASABLANCA_TESTS_DIR)/Common/utilities/include   
LDFLAGS=-L"$(OUTPUT_DIR)" $(LD_BASE_FLAGS)
LDLIBS=-lcasablanca -lunittestpp -l$(BOOST_SYSTEM) -l$(BOOST_THREAD) -lpthread -lcommon_utilities

OUTPUT_DIR?=./$(MODE)


# unsupported: pplx_agents

ifeq ($(UNAME),LINUX)
PKGCONFIG_CFLAGS = $(shell pkg-config libxml++-2.6 --cflags)
endif

$(OUTPUT_DIR)/pplx_test.so: pplx_op_test.cpp pplx_task_options.cpp pplxtask_tests.cpp pplx_scheduler.cpp stdafx.cpp 
	$(CXX) -fPIC -shared $(CXXFLAGS) $(PKGCONFIG_CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@


