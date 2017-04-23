set (SOURCES
  construction_tests.cpp
  negative_parsing_tests.cpp
  parsing_tests.cpp
  to_as_and_operators_tests.cpp
  iterator_tests.cpp
  json_numbers_tests.cpp
  stdafx.cpp
)
if (NOT WINDOWS_STORE AND NOT WINDOWS_PHONE)
  list(APPEND SOURCES fuzz_tests.cpp)
endif()

if(UNIX AND NOT APPLE)
  cpprest_find_boost()
  link_libraries(cpprestsdk_boost_internal)
endif()
add_casablanca_test(json_test SOURCES)
