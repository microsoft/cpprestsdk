if (NOT CPPREST_EXCLUDE_WEBSOCKETS)

  set(SOURCES
    utilities/test_websocket_server.cpp
  )

  add_library(websockettest_utilities ${SOURCES})
  target_include_directories(websockettest_utilities PUBLIC utilities)
  target_compile_definitions(websockettest_utilities PRIVATE -DWEBSOCKETTESTUTILITY_EXPORTS)
  if(NOT WIN32)
    target_compile_definitions(websockettest_utilities PRIVATE "-DWEBSOCKET_UTILITY_API=__attribute__ ((visibility (\"default\")))")
    target_compile_definitions(websockettest_utilities INTERFACE "-DWEBSOCKET_UTILITY_API=")
  endif()

  cpprest_find_websocketpp()
  target_link_libraries(websockettest_utilities
    PRIVATE
      unittestpp
      cpprest
      common_utilities
      cpprestsdk_websocketpp_internal
  )

  add_subdirectory(client)
endif()
