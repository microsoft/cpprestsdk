set(SOURCES
  fstreambuf_tests.cpp
  istream_tests.cpp
  memstream_tests.cpp
  ostream_tests.cpp
  stdstream_tests.cpp
  fuzz_tests.cpp
)
if (WIN32)
  list(APPEND SOURCES CppSparseFile.cpp)
endif ()
add_casablanca_test(streams_test SOURCES)
