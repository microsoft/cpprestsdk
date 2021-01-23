if (NOT WINDOWS_STORE AND NOT WINDOWS_PHONE)
  add_executable(oauth2client
    Oauth2Client.cpp
    )

  target_link_libraries(oauth2client cpprest)

  if(MINGW)
    target_link_libraries(oauth2client ws2_32 wsock32)
  endif()
endif()
