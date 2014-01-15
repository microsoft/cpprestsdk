set(NAME asyncop_test)

set(SOURCES
  fstreambuf_tests.cpp
  istream_tests.cpp
  memstream_tests.cpp
  ostream_tests.cpp
  stdstream_tests.cpp
)
set(LINKS
  ${LIB}casablanca
  ${LIB}unittestpp
  ${Boost_FRAMEWORK}
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  ${LIB}common_utilities
)

add_library(${LIB}${NAME} ${SOURCES})
target_link_libraries(${LIB}${NAME} ${LINKS})
add_test(NAME ${NAME}
  WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  COMMAND test_runner lib${NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
)
