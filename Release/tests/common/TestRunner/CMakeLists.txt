include_directories(${Casablanca_INCLUDE_DIRS}
  ../UnitTestpp
  )

set(TR_SOURCES
  test_runner.cpp
  test_module_loader.cpp
  )

add_definitions(-DDESKTOP_TEST_RUNNER)

if(NOT IOS AND NOT ANDROID)
  if(BUILD_SHARED_LIBS)
    add_executable(test_runner
      test_runner.cpp
      test_module_loader.cpp
      )

    target_link_libraries(test_runner
      ${Boost_LIBRARIES}
      unittestpp
      ${CMAKE_DL_LIBS}
      )
  else()
    if (APPLE)
      add_executable(test_runner
        test_runner.cpp
        test_module_loader.cpp
        )

      target_link_libraries(test_runner
        ${Boost_LIBRARIES}
        unittestpp
        ${CMAKE_DL_LIBS}
        -Wl,-force_load
        httpclient_test

        -Wl,-force_load
        json_test

        -Wl,-force_load
        uri_test

        -Wl,-force_load
        pplx_test

        -Wl,-force_load
        httplistener_test

        -Wl,-force_load
        streams_test

        -Wl,-force_load
        utils_test
        )
    elseif(UNIX)
      add_executable(test_runner
        test_runner.cpp
        test_module_loader.cpp
        )

      target_link_libraries(test_runner
        ${Boost_LIBRARIES}
        unittestpp
        ${CMAKE_DL_LIBS}
        -Wl,--whole-archive
        httpclient_test
        json_test
        uri_test
        pplx_test
        httplistener_test
        streams_test
        utils_test
        -Wl,--no-whole-archive
        )
    else()
      # In order to achieve --whole-archive on windows, we link all the test files into the test_runner directly
      # This means that the tests themselves must be created as "OBJECT" libraries
      add_executable(test_runner
        test_runner.cpp
        test_module_loader.cpp
        $<TARGET_OBJECTS:httpclient_test>
        $<TARGET_OBJECTS:json_test>
        $<TARGET_OBJECTS:uri_test>
        $<TARGET_OBJECTS:pplx_test>
        $<TARGET_OBJECTS:httplistener_test>
        $<TARGET_OBJECTS:streams_test>
        $<TARGET_OBJECTS:utils_test>
        )
      target_link_libraries(test_runner
        unittestpp
        ${CMAKE_DL_LIBS}
        )
    endif()
  endif()
endif()
