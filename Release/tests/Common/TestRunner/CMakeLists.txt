include_directories(${Casablanca_INCLUDE_DIR}
  ../UnitTestpp
  ${Boost_INCLUDE_DIR}
  )

set(TR_SOURCES
  test_runner.cpp
  test_module_loader.cpp
  )

add_definitions(-DDESKTOP_TEST_RUNNER)

if(NOT IOS)
  if(BUILD_SHARED_LIBS)
    add_executable(test_runner
      test_runner.cpp
      test_module_loader.cpp
      )

    target_link_libraries(test_runner
      ${Boost_FRAMEWORK}
      ${Boost_SYSTEM_LIBRARY}
      ${Boost_FILESYSTEM_LIBRARY}
      ${LIB}unittestpp
      ${CMAKE_DL_LIBS}
      )
  else()
    if (APPLE)
      add_executable(test_runner
        test_runner.cpp
        test_module_loader.cpp
        )

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
    elseif(UNIX)
      add_executable(test_runner
        test_runner.cpp
        test_module_loader.cpp
        )

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
    else()
      # In order to achieve --whole-archive on windows, we link all the test files into the test_runner directly
      # This means that the tests themselves must be created as "OBJECT" libraries
      add_executable(test_runner
        test_runner.cpp
        test_module_loader.cpp
        $<TARGET_OBJECTS:${LIB}httpclient_test>
        $<TARGET_OBJECTS:${LIB}json_test>
        $<TARGET_OBJECTS:${LIB}uri_test>
        $<TARGET_OBJECTS:${LIB}pplx_test>
        $<TARGET_OBJECTS:${LIB}httplistener_test>
        $<TARGET_OBJECTS:${LIB}asyncop_test>
        $<TARGET_OBJECTS:${LIB}utils_test>
        )
      target_link_libraries(test_runner
        ${LIB}unittestpp
        ${CMAKE_DL_LIBS}
        )
    endif()
  endif()

  add_custom_target(run_tests ALL DEPENDS run_tests.sh)
  add_custom_command(
    OUTPUT run_tests.sh
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/run_tests.sh $<TARGET_FILE_DIR:test_runner>
    DEPENDS run_tests.sh
    )
endif()	
