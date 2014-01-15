include_directories(
  ../utilities/include
  ../utilities/testlistener/include
)

set(NAME httpclient_test)

set(SOURCES
  authentication_tests.cpp
  building_request_tests.cpp
  client_construction.cpp
  connections_and_errors.cpp
  header_tests.cpp
  http_client_tests.cpp
  http_methods_tests.cpp
  multiple_requests.cpp
  outside_tests.cpp
  pipeline_stage_tests.cpp
  progress_handler_tests.cpp
  proxy_tests.cpp
  request_helper_tests.cpp
  request_stream_tests.cpp
  request_uri_tests.cpp
  response_extract_tests.cpp
  response_stream_tests.cpp
  status_code_reason_phrase_tests.cpp
  to_string_tests.cpp
)
set(LINKS
  ${LIB}httptest_utilities
  ${LIB}common_utilities
  ${Casablanca_LIBRARIES}
  ${LIB}unittestpp
  ${BOOST_SYSTEM_LIBRARY}
  ${BOOST_THREAD_LIBRARY}
)

add_library(${LIB}${NAME} ${SOURCES})

target_link_libraries(${LIB}${NAME} ${LINKS})

add_test(NAME ${NAME}
  WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  COMMAND test_runner lib${NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
)
