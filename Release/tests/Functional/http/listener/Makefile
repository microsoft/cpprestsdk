CXXFLAGS=$(LAX_BASE_CXXFLAGS) -I. -I$(CASABLANCA_INCLUDE_DIR) -I$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/ -I$(CASABLANCA_TESTS_DIR)/Common/utilities/include -I../utilities/include -include stdafx.h
LDFLAGS=-L"$(OUTPUT_DIR)" $(BASE_LDFLAGS) 
LDLIBS=-lhttptest_utilities -lcommon_utilities -lcasablanca -lunittestpp -l$(BOOST_SYSTEM) -l$(BOOST_THREAD) -lpthread

OUTPUT_DIR?=./$(MODE)

FILENAMES = \
	building_response_tests \
	connections_and_errors \
	header_tests \
	listener_construction_tests \
	pipeline_stage_tests \
	reply_helper_tests \
	request_extract_tests \
	request_handler_tests \
	request_relative_uri_tests \
	request_stream_tests \
	requests_tests \
	response_stream_tests \
	status_code_reason_phrase_tests \
	to_string_tests \
	stdafx

OBJECTS = $(addsuffix .$(MODE).o, $(FILENAMES))

ifeq ($(UNAME),LINUX)
PKGCONFIG_CFLAGS = $(shell pkg-config libxml++-2.6 --cflags)
endif

$(OUTPUT_DIR)/httplistener_test.so: $(OBJECTS)
	@echo "Building library $@"
	@$(CXX) -fPIC -shared $(CXXFLAGS) $(PKGCONFIG_CFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

-include *.$(MODE).d

$(OBJECTS): %.$(MODE).o: %.cpp
	@echo "Compiling $< -> $@"
	@$(CXX) -fPIC $(CXXFLAGS) -MMD -MF $*.$(MODE).d -c $< -o $@