set(SOURCES
  datetime.cpp
  base64.cpp
  strings.cpp
  macro_test.cpp
  nonce_generator_tests.cpp
  stdafx.cpp
)

add_casablanca_test(${LIB}utils_test SOURCES)

if(CMAKE_COMPILER_IS_GNUCXX)
  target_compile_options(${LIB}utils_test PRIVATE "-Wno-deprecated-declarations")
endif()