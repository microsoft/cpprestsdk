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

  add_casablanca_test(${LIB}websocketclient_test SOURCES)
  target_link_libraries(${LIB}websocketclient_test ${LIB}websockettest_utilities)
endif()