set(NAME uri_test)

set(SOURCES
  accessor_tests.cpp
  combining_tests.cpp
  constructor_tests.cpp
  conversions_tests.cpp
  diagnostic_tests.cpp
  encoding_tests.cpp
  operator_tests.cpp
  splitting_tests.cpp
  uri_builder_tests.cpp
)
set(LINKS
  ${LIB}casablanca
  ${LIB}unittestpp
  ${Boost_FRAMEWORK}
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_THREAD_LIBRARY}
)

add_library(${LIB}${NAME} ${SOURCES})
target_link_libraries(${LIB}${NAME} ${LINKS})
add_test(NAME ${NAME}
  WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  COMMAND test_runner lib${NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
)
