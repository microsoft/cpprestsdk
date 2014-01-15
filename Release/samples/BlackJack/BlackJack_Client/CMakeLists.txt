add_executable(blackjackclient
  ../../../collateral/Samples/BlackJack/BlackJack_Client/BlackJackClient.cpp
  ../../../collateral/Samples/BlackJack/BlackJack_Client/stdafx.cpp
  )

target_link_libraries(blackjackclient ${Casablanca_LIBRARIES})
