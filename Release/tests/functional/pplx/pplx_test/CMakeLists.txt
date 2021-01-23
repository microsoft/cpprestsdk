set(SOURCES
  pplx_op_test.cpp
  pplx_task_options.cpp
  pplxtask_tests.cpp
)

add_casablanca_test(pplx_test SOURCES)

configure_pch(pplx_test stdafx.h stdafx.cpp)

if(MINGW)
  target_link_libraries(pplx_test PRIVATE ws2_32 wsock32)
endif()
