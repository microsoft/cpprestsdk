include_directories(include)

if(WIN32)
  add_definitions(-DHTTPTESTUTILITY_EXPORTS)
endif()

set(SOURCES
  http_asserts.cpp
  test_http_client.cpp
  test_http_server.cpp
  test_server_utilities.cpp
)

add_library(httptest_utilities ${SOURCES})
target_link_libraries(httptest_utilities
  unittestpp
  common_utilities
  ${BOOST_LIBRARIES}
  ${Casablanca_LIBRARIES}
)
