#!/bin/bash

DEBUGGER=""
ENABLE_DEBUG_MODE=""
if [ "$1" = "--debug" ]; then
    DEBUGGER="gdb -args" # -q -x automate_gdb -args"
    ENABLE_DEBUG_MODE="/breakonerror"
fi

export LD_LIBRARY_PATH=.

exit_code=0

for test_lib in *_test.so; do
    $DEBUGGER ./test_runner $ENABLE_DEBUG_MODE $test_lib 
    if [ $? -ne 0 ]
    then
        echo "Test suite failed"
        exit_code=1
    fi
done
if [ $exit_code -eq 0 ];
then
    echo "All tests finished successfully"
else
    echo "Some tests failed"
fi
date
exit $exit_code

