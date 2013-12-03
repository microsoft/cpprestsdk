CXXFLAGS=$(BASE_CXXFLAGS) -I"$(CASABLANCA_INCLUDE_DIR)" -I$(CASABLANCA_TESTS_DIR)/Common/UnitTestpp/
LDFLAGS=-L"$(OUTPUT_DIR)" $(BASE_LDFLAGS)
LDLIBS=-lcasablanca -lunittestpp -l$(BOOST_SYSTEM) -l$(BOOST_THREAD) -l$(BOOST_REGEX) -lpthread -lstdc++ -lm

OUTPUT_DIR?=./$(MODE)

FILENAME = $(OUTPUT_DIR)/json_test

-include $(FILENAME).d

ifeq ($(UNAME),LINUX)
PKGCONFIG_CFLAGS = $(shell pkg-config libxml++-2.6 --cflags)
endif

$(FILENAME).so: \
	construction_tests.cpp \
	negative_parsing_tests.cpp \
	parsing_tests.cpp \
	to_as_and_operators_tests.cpp
	$(CXX) -fPIC -shared $(CXXFLAGS) $(PKGCONFIG_CFLAGS) $(LDFLAGS) -MMD -MF $(FILENAME).d $(filter %.cpp, $^) $(LDLIBS) -o $@ 


