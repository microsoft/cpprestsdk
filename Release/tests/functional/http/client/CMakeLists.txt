include_directories(
  ../utilities/include
  ../utilities/testlistener/include
  ${Casablanca_INCLUDE_DIRS}
)

set(SOURCES
  authentication_tests.cpp
  building_request_tests.cpp
  client_construction.cpp
  connections_and_errors.cpp
  header_tests.cpp
  http_client_tests.cpp
  http_methods_tests.cpp
  multiple_requests.cpp
  oauth1_tests.cpp
  oauth2_tests.cpp
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
  http_client_fuzz_tests.cpp
)

add_casablanca_test(httpclient_test SOURCES)
