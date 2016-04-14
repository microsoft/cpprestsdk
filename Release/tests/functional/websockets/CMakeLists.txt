if (NOT CPPREST_EXCLUDE_WEBSOCKETS)

  include_directories(utilities/include)
  
  set(SOURCES
    utilities/test_websocket_server.cpp
  )
  
  add_library(websockettest_utilities ${TEST_LIBRARY_TARGET_TYPE} ${SOURCES})
  target_compile_definitions(websockettest_utilities PRIVATE -DWEBSOCKETTESTUTILITY_EXPORTS)
  if (NOT TEST_LIBRARY_TARGET_TYPE STREQUAL "OBJECT")
    target_link_libraries(websockettest_utilities
      unittestpp
      common_utilities
      ${BOOST_LIBRARIES}
      ${Casablanca_LIBRARIES}
    )
  endif()

  add_subdirectory(client)
endif()
