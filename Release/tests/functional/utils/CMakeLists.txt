set(SOURCES
  datetime.cpp
  base64.cpp
  strings.cpp
  macro_test.cpp
  nonce_generator_tests.cpp
)

add_casablanca_test(${LIB}utils_test SOURCES)
