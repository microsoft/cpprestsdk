set(SOURCES
  fstreambuf_tests.cpp
  istream_tests.cpp
  memstream_tests.cpp
  ostream_tests.cpp
  stdstream_tests.cpp
)
if (NOT WINDOWS_STORE AND NOT WINDOWS_PHONE)
  list(APPEND SOURCES fuzz_tests.cpp)
  if (WIN32)
    list(APPEND SOURCES CppSparseFile.cpp)
  endif ()
else ()
  list(APPEND SOURCES winrt_interop_tests.cpp)
endif ()
add_casablanca_test(streams_test SOURCES)
