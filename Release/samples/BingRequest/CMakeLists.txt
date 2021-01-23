if (NOT WINDOWS_STORE AND NOT WINDOWS_PHONE)
  add_executable(BingRequest bingrequest.cpp)
  target_link_libraries(BingRequest cpprest)

  if(MINGW)
    target_link_libraries(BingRequest ws2_32 wsock32)
  endif()
endif()