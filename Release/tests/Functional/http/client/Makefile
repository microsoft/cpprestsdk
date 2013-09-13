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

FILENAMES = \
	authentication_tests \
	building_request_tests \
	client_construction \
	connections_and_errors \
	header_tests \
	http_client_tests \
	http_methods_tests \
	multiple_requests \
	outside_tests \
	pipeline_stage_tests \
	progress_handler_tests \
	proxy_tests \
	request_helper_tests \
	request_stream_tests \
	request_uri_tests \
	response_extract_tests \
	response_stream_tests \
	status_code_reason_phrase_tests \
	to_string_tests

OBJECTS = $(addsuffix .$(MODE).o, $(FILENAMES))

$(OUTPUT_DIR)/httpclient_test.so: $(OBJECTS)
	@echo "Building library $@"
	@$(CXX) -fPIC -shared $(CXXFLAGS) $(shell pkg-config libxml++-2.6 --cflags) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJECTS): %.$(MODE).o: %.cpp
	@echo "Compiling $< -> $@"
	@$(CXX) -fPIC $(CXXFLAGS) -c $< -o $@