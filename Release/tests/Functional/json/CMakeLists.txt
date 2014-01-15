set(NAME json_test)

set (SOURCES
  construction_tests.cpp
  negative_parsing_tests.cpp
  parsing_tests.cpp
  to_as_and_operators_tests.cpp
)
set(LINKS
  ${LIB}casablanca
  ${LIB}unittestpp
  ${Boost_FRAMEWORK}
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  ${Boost_REGEX_LIBRARY}
)

add_library(${LIB}${NAME} ${SOURCES})
target_link_libraries(${LIB}${NAME} ${LINKS})
add_test(NAME ${NAME}
  WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  COMMAND test_runner lib${NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
)
