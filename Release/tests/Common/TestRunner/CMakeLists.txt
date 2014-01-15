include_directories(${Casablanca_INCLUDE_DIR}
  ../UnitTestpp
  ${Boost_INCLUDE_DIR}
  )

set(TR_SOURCES
  test_runner.cpp
  test_module_loader.cpp
  )

if(WIN32)
  add_definitions(-DDESKTOP_TEST_RUNNER -DWIN32 -DNDEBUG -D_CONSOLE)
endif()

if(NOT IOS)
  add_executable(test_runner
    test_runner.cpp
    test_module_loader.cpp
  )
  if(NOT BUILD_SHARED_LIBS)
    if (APPLE)
      target_link_libraries(test_runner
        ${Boost_FRAMEWORK}
        ${Boost_SYSTEM_LIBRARY}
        ${Boost_FILESYSTEM_LIBRARY}
        ${LIB}unittestpp
        ${CMAKE_DL_LIBS}
        -Wl,-force_load
        ${LIB}httpclient_test

        -Wl,-force_load
        ${LIB}json_test

        -Wl,-force_load
        ${LIB}uri_test

        -Wl,-force_load
        ${LIB}pplx_test

        -Wl,-force_load
        ${LIB}httplistener_test

        -Wl,-force_load
        ${LIB}asyncop_test

        -Wl,-force_load
        ${LIB}utils_test
        )
    else()
      target_link_libraries(test_runner
        ${Boost_FRAMEWORK}
        ${Boost_SYSTEM_LIBRARY}
        ${Boost_FILESYSTEM_LIBRARY}
        ${LIB}unittestpp
        ${CMAKE_DL_LIBS}
        -Wl,--whole-archive
        ${LIB}httpclient_test
        ${LIB}json_test
        ${LIB}uri_test
        ${LIB}pplx_test
        ${LIB}httplistener_test
        ${LIB}asyncop_test
        ${LIB}utils_test
        -Wl,--no-whole-archive
        )
    endif()
  else()
    target_link_libraries(test_runner
      ${Boost_FRAMEWORK}
      ${Boost_SYSTEM_LIBRARY}
      ${Boost_FILESYSTEM_LIBRARY}
      ${LIB}unittestpp
      ${CMAKE_DL_LIBS}
      )
  endif()
endif()	
