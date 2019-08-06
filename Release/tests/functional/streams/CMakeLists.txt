set(SOURCES
  fstreambuf_tests.cpp
  istream_tests.cpp
  memstream_tests.cpp
  ostream_tests.cpp
  stdstream_tests.cpp
)
if(WINDOWS_STORE OR WINDOWS_PHONE)
  list(APPEND SOURCES winrt_interop_tests.cpp)
else()
  list(APPEND SOURCES fuzz_tests.cpp)
  if(WIN32)
    list(APPEND SOURCES CppSparseFile.cpp)
  endif()
endif()

add_casablanca_test(streams_test SOURCES cpprest_streams)
if(NOT WIN32 OR CPPREST_WEBSOCKETS_IMPL STREQUAL "wspp")
  cpprest_find_boost()
  if(NOT TEST_LIBRARY_TARGET_TYPE STREQUAL "OBJECT")
    target_link_libraries(streams_test PRIVATE cpprestsdk_boost_internal)
  else()
    target_include_directories(streams_test PRIVATE $<TARGET_PROPERTY:cpprestsdk_boost_internal,INTERFACE_INCLUDE_DIRECTORIES>)
  endif()
endif()

if(MSVC)
  get_target_property(_srcs streams_test SOURCES)

  if(NOT CMAKE_GENERATOR MATCHES "Visual Studio .*")
    set_property(SOURCE stdafx.cpp APPEND PROPERTY OBJECT_OUTPUTS "${CMAKE_CURRENT_BINARY_DIR}/streams-tests-stdafx.pch")
    set_property(SOURCE ${_srcs} APPEND PROPERTY OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/streams-tests-stdafx.pch")
  endif()

  set_source_files_properties(stdafx.cpp PROPERTIES COMPILE_FLAGS "/Ycstdafx.h /Fpstreams-tests-stdafx.pch")
  target_sources(streams_test PRIVATE stdafx.cpp)
  target_compile_options(streams_test PRIVATE /Yustdafx.h /Fpstreams-tests-stdafx.pch)
endif()
