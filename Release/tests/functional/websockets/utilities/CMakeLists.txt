if (NOT CPPREST_EXCLUDE_WEBSOCKETS)

  include_directories(include)
  
  if(WIN32)
    add_definitions(-DWEBSOCKETTESTUTILITY_EXPORTS)
  endif()
  
  set(SOURCES
    test_websocket_server.cpp
  )
  
  add_library(${LIB}websockettest_utilities ${SOURCES})
  target_link_libraries(${LIB}websockettest_utilities
    ${LIB}unittestpp
    ${LIB}common_utilities
    ${BOOST_LIBRARIES}
    ${Casablanca_LIBRARIES}
  )

endif()