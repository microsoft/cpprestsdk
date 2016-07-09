if (NOT CPPREST_EXCLUDE_WEBSOCKETS)
  include_directories(../utilities)
  
  set(SOURCES
    authentication_tests.cpp
    client_construction.cpp
    close_tests.cpp
    error_tests.cpp
    receive_msg_tests.cpp
    send_msg_tests.cpp
  )

  add_casablanca_test(websocketclient_test SOURCES)
  if (NOT TEST_LIBRARY_TARGET_TYPE STREQUAL "OBJECT")
    target_link_libraries(websocketclient_test websockettest_utilities)
  endif()
endif()