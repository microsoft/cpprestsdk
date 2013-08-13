LIBRARY=libcasablanca.so

SOURCES = \
	streams/linux/fileio_linux.cpp \
	json/json.cpp \
	utilities/asyncrt_utils.cpp \
	pplx/linux/pplxlinux.cpp \
        pplx/pplx.cpp \
	pplx/threadpool.cpp \
	http/uri/uri.cpp \
	http/uri/uri_builder.cpp \
	http/uri/uri_parser.cpp \
	http/common/http_msg.cpp \
	http/common/http_helpers.cpp \
	http/client/http_msg_client.cpp \
	http/client/http_linux.cpp \
	http/listener/http_linux_server.cpp \
	http/listener/http_listener.cpp \
	http/listener/http_msg_listen.cpp \
	http/listener/http_server_api.cpp \
	http/logging/log.cpp \
	utilities/base64.cpp

HEADERS = \
	../include/cpprest/astreambuf.h \
	../include/cpprest/streams.h \
	../include/cpprest/asyncrt_utils.h \
	../include/cpprest/base_uri.h \
	../include/cpprest/basic_types.h \
	../include/cpprest/containerstream.h \
	../include/cpprest/fileio.h \
	../include/cpprest/filelog.h \
	../include/cpprest/filestream.h \
	../include/cpprest/globals.h \
	../include/cpprest/genstreambuf.h \
	../include/cpprest/http_client.h \
	../include/cpprest/http_client_impl.h \
	../include/cpprest/http_constants.dat \
	../include/cpprest/http_helpers.h \
	../include/cpprest/http_linux_server.h \
	../include/cpprest/http_listener.h \
	../include/cpprest/http_msg.h \
	../include/cpprest/http_server.h \
	../include/cpprest/http_server_api.h \
	../include/cpprest/interopstream.h \
	../include/cpprest/producerconsumerstream.h \
	../include/cpprest/rawptrstream.h \
	../include/cpprest/json.h \
	../include/cpprest/log.h \
	../include/pplx/pplxconv.h \
	../include/pplx/pplxlinux.h \
	../include/pplx/pplxinterface.h \
	../include/pplx/pplx.h \
	../include/pplx/pplxtasks.h \
	../include/pplx/pplxcancellation_token.h \
	../include/cpprest/uri.h \
	../include/cpprest/uri_builder.h \
	../include/cpprest/uri_parser.h \
	../include/cpprest/xxpublic.h \
	../include/compat/linux_compat.h \
	../include/compat/SafeInt3.hpp \
	../include/pplx/threadpool.h \
	../src/pch/stdafx.h

PCH = ./pch/stdafx.h

PRECOMPILED_HEADERS = $(addsuffix .gch, $(PCH))
OBJECTS = $(addsuffix .$(MODE).o, $(basename $(SOURCES)))

# flags for the C++ compiler

PKGCONFIG_CFLAGS = $(shell pkg-config libxml++-2.6 --cflags) 
PKGCONFIG_LIBS = $(shell pkg-config libxml++-2.6 --libs) $(shell pkg-config libssl --libs)

#-fpch-deps -MMD not useful with a single monolithic PCH

CXXFLAGS = -fPIC -std=c++11 $(STRICT_BASE_CXXFLAGS) -I../include -I./pch $(WARNINGS) $(PKGCONFIG_CFLAGS)
LIBS = $(PKGCONFIG_LIBS) -lboost_system -lboost_thread -lboost_locale -pthread -lstdc++ -lm # these are explicit for clang
LDFLAGS = $(BASE_LDFLAGS)

ifeq ($(OUTPUT_DIR),)
  LIBRARY_DST=$(MODE)/$(LIBRARY)
else
  LIBRARY_DST=$(OUTPUT_DIR)/$(LIBRARY)
endif

all:  $(LIBRARY_DST)

%.h.gch: %.h
	@echo "Building precompiled header $(@)"
	@$(CXX) -x c++-header $(CXXFLAGS) -c $< -o $@

%.$(MODE).o: %.cpp
	@echo "Compiling $< -> $@"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(PRECOMPILED_HEADERS): $(HEADERS)

$(LIBRARY_DST): $(OBJECTS)
	@echo "Building library $@"
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared $^ -o $@ $(LIBS)

#DEPS := $(OBJECTS:.o=.d)
#-include $(DEPS)

clean:
	rm $(OBJECTS) $(PRECOMPILED_HEADERS) $(LIBRARY)

.SILENT:clean
