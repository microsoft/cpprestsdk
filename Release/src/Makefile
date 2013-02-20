LIBRARY=libcasablanca.so

SOURCES = \
	http/client/http_msg_client.cpp \
	http/client/http_client.cpp \
	http/common/http_msg.cpp \
	http/common/http_helpers.cpp \
	streams/linux/fileio_linux.cpp \
	json/json.cpp \
	utilities/asyncrt_utils.cpp \
	pplx/linux/pplxlinux.cpp \
    pplx/pplx.cpp \
	pplx/threadpool.cpp \
	http/uri/uri.cpp \
	http/uri/uri_builder.cpp \
	http/uri/uri_parser.cpp

HEADERS = \
	../include/astreambuf.h \
	../include/astream.h \
	../include/asyncrt_utils.h \
	../include/cancellation_token.h \
	../include/filelog.h \
	../include/genstreambuf.h \
	../include/http_client.h \
	../include/http_constants.dat \
	../include/http_lib.h \
	../include/http_msg.h \
	../include/json.h \
    ../include/pplxatomics.h \
    ../include/pplxlinux.h \
	../include/pplxdefs.h \
	../include/pplx.h \
	../include/pplxtasks.h \
	../include/serialization.h \
	../include/uri.h \
	../include/fileio.h \
	../include/xxpublic.h \
	../include/linux_compat.h \
	../include/SafeInt3.hpp \
	../include/threadpool.h \
	../include/stdafx.h

PCH = include/stdafx.h

PRECOMPILED_HEADERS = $(addsuffix .gch, $(PCH))
OBJECTS = $(addsuffix .$(MODE).o, $(basename $(SOURCES)))

# flags for the C++ compiler

PKGCONFIG_CFLAGS = $(shell pkg-config libxml++-2.6 --cflags) 
PKGCONFIG_LIBS = $(shell pkg-config libxml++-2.6 --libs) $(shell pkg-config libssl --libs)

#-fpch-deps -MMD not useful with a single monolithic PCH

CXXFLAGS = -fPIC $(STRICT_BASE_CXXFLAGS) -I../include -I../include/pch $(WARNINGS) $(PKGCONFIG_CFLAGS)
LIBS = $(PKGCONFIG_LIBS) -lboost_system -lboost_thread -lboost_locale -pthread -lstdc++ -lm # these are explicit for clang
LDFLAGS = $(BASE_LDFLAGS)

CXX ?= g++-4.7

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
