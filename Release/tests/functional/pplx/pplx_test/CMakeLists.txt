set(SOURCES
  pplx_op_test.cpp
  pplx_task_options.cpp
  pplxtask_tests.cpp
)

add_casablanca_test(pplx_test SOURCES)

if(MSVC)
  get_target_property(_srcs pplx_test SOURCES)

  if(NOT CMAKE_GENERATOR MATCHES "Visual Studio .*")
    set_property(SOURCE stdafx.cpp APPEND PROPERTY OBJECT_OUTPUTS "${CMAKE_CURRENT_BINARY_DIR}/pplx-tests-stdafx.pch")
    set_property(SOURCE ${_srcs} APPEND PROPERTY OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pplx-tests-stdafx.pch")
  endif()

  set_source_files_properties(stdafx.cpp PROPERTIES COMPILE_FLAGS "/Ycstdafx.h /Fppplx-tests-stdafx.pch")
  target_sources(pplx_test PRIVATE stdafx.cpp)
  target_compile_options(pplx_test PRIVATE /Yustdafx.h /Fppplx-tests-stdafx.pch)
endif()
