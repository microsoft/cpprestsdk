CXXFLAGS=$(LAX_BASE_CXXFLAGS) -I../../../include/ -I../UnitTestpp/
LDFLAGS=-L../UnitTestpp/lib/linux_gcc/
LDLIBS=-lboost_system -lboost_filesystem -lunittestpp -ldl

.PHONY: all

all: $(OUTPUT_DIR)/test_runner $(OUTPUT_DIR)/run_tests.sh

$(OUTPUT_DIR)/test_runner: test_runner
	cp test_runner $@

$(OUTPUT_DIR)/run_tests.sh: run_tests.sh
	cp run_tests.sh $@
	
test_runner: test_runner.cpp test_module_loader.cpp

