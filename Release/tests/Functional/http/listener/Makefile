CXXFLAGS=$(LAX_BASE_CXXFLAGS) -I. -I$(CASABLANCA_INCLUDE_DIR) -I$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/ -I$(CASABLANCA_TESTS_DIR)/Common/utilities/include -I../utilities/include -include stdafx.h
LDFLAGS=-L../../../Common/UnitTestpp/lib/linux_gcc/ -L"$(OUTPUT_DIR)" $(BASE_LDFLAGS) 
LDLIBS=-lhttptest_utilities -lcommon_utilities -lcasablanca -lunittestpp -lboost_system -lboost_thread -lpthread

OUTPUT_DIR?=./$(MODE)

$(OUTPUT_DIR)/httplistener_test.so: \
	building_response_tests.cpp \
	connections_and_errors.cpp \
	header_tests.cpp \
	listener_construction_tests.cpp \
	pipeline_stage_tests.cpp \
	reply_helper_tests.cpp \
	request_extract_tests.cpp \
	request_handler_tests.cpp \
	request_relative_uri_tests.cpp \
	request_stream_tests.cpp \
	requests_tests.cpp \
	response_stream_tests.cpp \
	status_code_reason_phrase_tests.cpp \
	to_string_tests.cpp \
	stdafx.cpp
	$(CXX) -fPIC -shared $(CXXFLAGS) $(shell pkg-config libxml++-2.6 --cflags) $(LDFLAGS) $^ $(LDLIBS) -o $@ 


