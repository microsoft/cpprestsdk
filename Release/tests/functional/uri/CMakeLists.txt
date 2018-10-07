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
  stdafx.cpp
)

add_casablanca_test(uri_test SOURCES)

if(MSVC)
  get_target_property(_srcs uri_test SOURCES)

  if(NOT CMAKE_GENERATOR MATCHES "Visual Studio .*")
    set_property(SOURCE stdafx.cpp APPEND PROPERTY OBJECT_OUTPUTS "${CMAKE_CURRENT_BINARY_DIR}/stdafx.pch")
    set_property(SOURCE ${_srcs} APPEND PROPERTY OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/stdafx.pch")
  endif()

  set_source_files_properties(stdafx.cpp PROPERTIES COMPILE_FLAGS "/Ycstdafx.h")
  target_sources(uri_test PRIVATE stdafx.cpp)
  target_compile_options(uri_test PRIVATE /Yustdafx.h /Zm200)
endif()
