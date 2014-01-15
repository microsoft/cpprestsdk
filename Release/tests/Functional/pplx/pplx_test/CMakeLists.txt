set(NAME pplx_test)

set(SOURCES
  pplx_op_test.cpp
  pplx_task_options.cpp
  pplxtask_tests.cpp
  pplx_scheduler.cpp
  stdafx.cpp
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
