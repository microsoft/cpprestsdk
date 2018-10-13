set(SOURCES
  construction_tests.cpp
  negative_parsing_tests.cpp
  parsing_tests.cpp
  to_as_and_operators_tests.cpp
  iterator_tests.cpp
  json_numbers_tests.cpp
)
if(NOT WINDOWS_STORE AND NOT WINDOWS_PHONE)
  list(APPEND SOURCES fuzz_tests.cpp)
endif()

add_casablanca_test(json_test SOURCES)
if(UNIX AND NOT APPLE)
  cpprest_find_boost()
  target_link_libraries(json_test PRIVATE cpprestsdk_boost_internal)
endif()

if(MSVC)
  get_target_property(_srcs json_test SOURCES)

  if(NOT CMAKE_GENERATOR MATCHES "Visual Studio .*")
    set_property(SOURCE stdafx.cpp APPEND PROPERTY OBJECT_OUTPUTS "${CMAKE_CURRENT_BINARY_DIR}/json-tests-stdafx.pch")
    set_property(SOURCE ${_srcs} APPEND PROPERTY OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/json-tests-stdafx.pch")
  endif()

  set_source_files_properties(stdafx.cpp PROPERTIES COMPILE_FLAGS "/Ycstdafx.h /Fpjson-tests-stdafx.pch")
  target_sources(json_test PRIVATE stdafx.cpp)
  target_compile_options(json_test PRIVATE /Yustdafx.h /Fpjson-tests-stdafx.pch)
endif()
