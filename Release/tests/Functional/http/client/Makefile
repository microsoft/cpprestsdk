CXXFLAGS=$(LAX_BASE_CXXFLAGS) \
    -I$(CASABLANCA_INCLUDE_DIR) \
    -I$(CASABLANCA_TESTS_DIR)/Functional/http/utilities/include/ \
    -I$(CASABLANCA_TESTS_DIR)/Functional/http/utilities/testlistener/include/ \
    -I$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/ \
    -I$(CASABLANCA_TESTS_DIR)/Common/utilities/include/ \
    -include stdafx.h
LDFLAGS=-L$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/lib/linux_gcc/ -L"$(OUTPUT_DIR)" $(BASE_LDFLAGS) 
LDLIBS=-lhttptest_utilities -lcommon_utilities -lcasablanca -lunittestpp -lboost_system -lboost_thread -lpthread

OUTPUT_DIR?=./$(MODE)

$(OUTPUT_DIR)/httpclient_test.so: \
	authentication_tests.cpp \
	building_request_tests.cpp \
	client_construction.cpp \
	connections_and_errors.cpp \
	header_tests.cpp \
	http_client_tests.cpp \
	http_methods_tests.cpp \
	multiple_requests.cpp \
	outside_tests.cpp \
	pipeline_stage_tests.cpp \
	progress_handler_tests.cpp \
	proxy_tests.cpp \
	request_helper_tests.cpp \
	request_stream_tests.cpp \
	request_uri_tests.cpp \
	response_extract_tests.cpp \
	response_stream_tests.cpp \
	status_code_reason_phrase_tests.cpp \
	to_string_tests.cpp
	$(CXX) -fPIC -shared $(CXXFLAGS) $(shell pkg-config libxml++-2.6 --cflags) $(LDFLAGS) $^ $(LDLIBS) -o $@ 


