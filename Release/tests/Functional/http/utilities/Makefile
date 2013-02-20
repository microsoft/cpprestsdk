CXXFLAGS=$(LAX_BASE_CXXFLAGS) -I. -I$(CASABLANCA_INCLUDE_DIR) -I$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/ -I./testlistener/include -Iinclude -include linux_compat.h
LDFLAGS=-L$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/lib/linux_gcc/ -L"$(OUTPUT_DIR)" $(BASE_LDFLAGS)
LDLIBS=-lcasablanca -lunittestpp -lboost_system -lboost_thread -lboost_locale -lpthread -lstdc++

OUTPUT_DIR?=./$(MODE)

$(OUTPUT_DIR)/libhttptest_utilities.so: \
	http_asserts.cpp \
	test_http_client.cpp \
	test_http_server.cpp \
	test_server_utilities.cpp \
	testlistener/src/listener/http_linux_server.cpp \
	testlistener/src/listener/http_listener.cpp \
	testlistener/src/listener/http_msg_listen.cpp \
	testlistener/src/listener/http_server_api.cpp \
	testlistener/src/logging/log.cpp 
	$(CXX) $(CXXFLAGS) -fPIC -shared $(shell pkg-config libxml++-2.6 --cflags) $(LDFLAGS) $^ $(LDLIBS) -o $@
