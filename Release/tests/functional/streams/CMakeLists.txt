set(SOURCES
  fstreambuf_tests.cpp
  istream_tests.cpp
  memstream_tests.cpp
  ostream_tests.cpp
  stdstream_tests.cpp
  stdafx.cpp
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

if(NOT CPPREST_WEBSOCKETS_IMPL STREQUAL "none")
  cpprest_find_boost()
  target_include_directories(streams_test PRIVATE ${Boost_INCLUDE_DIR})
endif()