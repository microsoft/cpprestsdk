if (NOT WINDOWS_STORE AND NOT WINDOWS_PHONE)
  add_executable(oauth1client
    Oauth1Client.cpp
    )

  target_link_libraries(oauth1client cpprest)

  if(MINGW)
    target_link_libraries(oauth1client ws2_32 wsock32)
  endif()
endif()
