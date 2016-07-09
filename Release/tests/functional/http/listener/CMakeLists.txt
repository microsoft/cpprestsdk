if (NOT WINDOWS_STORE AND NOT WINDOWS_PHONE)
  include_directories (../utilities/include)
  
  set (SOURCES
    building_response_tests.cpp
    connections_and_errors.cpp
    header_tests.cpp
    listener_construction_tests.cpp
    reply_helper_tests.cpp
    request_extract_tests.cpp
    request_handler_tests.cpp
    request_relative_uri_tests.cpp
    request_stream_tests.cpp
    requests_tests.cpp
    response_stream_tests.cpp
    status_code_reason_phrase_tests.cpp
    to_string_tests.cpp
    stdafx.cpp
  )
  
  add_casablanca_test(httplistener_test SOURCES)
endif ()