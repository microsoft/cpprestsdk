set(SOURCES
  datetime.cpp
  base64.cpp
  strings.cpp
  macro_test.cpp
  nonce_generator_tests.cpp
  win32_encryption_tests.cpp
)

add_casablanca_test(utils_test SOURCES cpprest_utilities)

if(CMAKE_COMPILER_IS_GNUCXX)
  target_compile_options(utils_test PRIVATE "-Wno-deprecated-declarations")
endif()

if(MSVC)
  get_target_property(_srcs utils_test SOURCES)

  if(NOT CMAKE_GENERATOR MATCHES "Visual Studio .*")
    set_property(SOURCE stdafx.cpp APPEND PROPERTY OBJECT_OUTPUTS "${CMAKE_CURRENT_BINARY_DIR}/utils-tests-stdafx.pch")
    set_property(SOURCE ${_srcs} APPEND PROPERTY OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/utils-tests-stdafx.pch")
  endif()

  set_source_files_properties(stdafx.cpp PROPERTIES COMPILE_FLAGS "/Ycstdafx.h /Fputils-tests-stdafx.pch")
  target_sources(utils_test PRIVATE stdafx.cpp)
  target_compile_options(utils_test PRIVATE /Yustdafx.h /Fputils-tests-stdafx.pch)
endif()
