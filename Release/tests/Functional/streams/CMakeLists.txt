set(SOURCES
  fstreambuf_tests.cpp
  istream_tests.cpp
  memstream_tests.cpp
  ostream_tests.cpp
  stdstream_tests.cpp
  fuzz_tests.cpp
)
add_casablanca_test(${LIB}asyncop_test SOURCES)
