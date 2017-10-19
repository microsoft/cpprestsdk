if (NOT CPPREST_EXCLUDE_WEBSOCKETS)
  set(SOURCES
    authentication_tests.cpp
    client_construction.cpp
    close_tests.cpp
    error_tests.cpp
    receive_msg_tests.cpp
    send_msg_tests.cpp
    stdafx.cpp
  )

  # This works around "OBJECT" tests
  link_libraries(websockettest_utilities)
  add_casablanca_test(websocketclient_test SOURCES)
endif()