add_executable(blackjackclient
  BlackJackClient.cpp
  stdafx.cpp
  )

target_link_libraries(blackjackclient ${Casablanca_LIBRARIES})
