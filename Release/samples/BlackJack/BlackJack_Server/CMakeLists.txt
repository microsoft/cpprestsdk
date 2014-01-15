add_definitions(-Wno-sign-compare -Wno-enum-compare)

add_executable(blackjackserver
  ../../../collateral/Samples/BlackJack/BlackJack_Server/BlackJack_Server.cpp
  ../../../collateral/Samples/BlackJack/BlackJack_Server/Dealer.cpp
  ../../../collateral/Samples/BlackJack/BlackJack_Server/stdafx.cpp
  ../../../collateral/Samples/BlackJack/BlackJack_Server/Table.cpp
  )

target_link_libraries(blackjackserver ${Casablanca_LIBRARIES})
