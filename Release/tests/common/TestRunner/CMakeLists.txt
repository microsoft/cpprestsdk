include_directories(${Casablanca_INCLUDE_DIRS}
  ../UnitTestpp
  )

set(TR_SOURCES
  test_runner.cpp
  test_module_loader.cpp
  )

if (WIN32)
  if (WINDOWS_STORE OR WINDOWS_PHONE)
    add_definitions(-DWINRT_TEST_RUNNER -D_CONSOLE)
  else()
    add_definitions(-DDESKTOP_TEST_RUNNER)
  endif()
endif()

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
      set(SOURCES
        test_runner.cpp
        $<TARGET_OBJECTS:httpclient_test>
        $<TARGET_OBJECTS:json_test>
        $<TARGET_OBJECTS:uri_test>
        $<TARGET_OBJECTS:pplx_test>
        $<TARGET_OBJECTS:streams_test>
        $<TARGET_OBJECTS:utils_test>
      )
      if (NOT WINDOWS_STORE AND NOT WINDOWS_PHONE)
        list(APPEND SOURCES
          test_module_loader.cpp
          $<TARGET_OBJECTS:httplistener_test>
        )
      endif()
      add_executable(test_runner ${SOURCES})
      target_link_libraries(test_runner
        unittestpp
        common_utilities
        httptest_utilities
        cpprest
        ${CMAKE_DL_LIBS}
        )
      if (WINDOWS_STORE)
        target_link_libraries(test_runner ucrtd.lib vcruntimed.lib vccorlibd.lib msvcrtd.lib msvcprtd.lib concrtd.lib RuntimeObject.lib)
      endif()
    endif()
  endif()
endif()
