include_directories(include)

add_definitions(-DHTTPTESTUTILITY_EXPORTS)

set(SOURCES
  http_asserts.cpp
  test_http_client.cpp
  test_http_server.cpp
  test_server_utilities.cpp
)

add_library(${LIB}httptest_utilities ${TEST_LIBRARY_TARGET_TYPE} ${SOURCES})
target_link_libraries(${LIB}httptest_utilities
  ${LIB}unittestpp
  ${LIB}common_utilities
  ${BOOST_LIBRARIES}
  ${Casablanca_LIBRARIES}
)
