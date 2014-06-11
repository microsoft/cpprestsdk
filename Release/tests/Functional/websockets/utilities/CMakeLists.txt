include_directories(include)

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
