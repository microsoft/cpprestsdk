if (UNIX)
  add_definitions(-Wno-switch)
endif()

add_executable(blackjackclient
  BlackJackClient.cpp
  )

target_link_libraries(blackjackclient cpprest)

if(MINGW)
  target_link_libraries(blackjackclient ws2_32 wsock32)
endif()
