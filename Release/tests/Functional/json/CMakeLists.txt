set (SOURCES
  construction_tests.cpp
  negative_parsing_tests.cpp
  parsing_tests.cpp
  to_as_and_operators_tests.cpp
)

add_casablanca_test(${LIB}json_test SOURCES)
