set(SOURCES
  pplx_op_test.cpp
  pplx_task_options.cpp
  pplxtask_tests.cpp
  pplx_scheduler.cpp
  stdafx.cpp
)

add_casablanca_test(${LIB}pplx_test SOURCES)
