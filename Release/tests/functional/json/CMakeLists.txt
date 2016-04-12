set (SOURCES
  construction_tests.cpp
  negative_parsing_tests.cpp
  parsing_tests.cpp
  to_as_and_operators_tests.cpp
  iterator_tests.cpp
  json_numbers_tests.cpp
)
if (NOT WINDOWS_STORE AND NOT WINDOWS_PHONE)
  list(APPEND SOURCES fuzz_tests.cpp)
endif()

add_casablanca_test(json_test SOURCES)
