add_definitions(-Wno-sign-compare -Wno-enum-compare)

add_executable(blackjackserver
  BlackJack_Server.cpp
  Dealer.cpp
  stdafx.cpp
  Table.cpp
  )

target_link_libraries(blackjackserver ${Casablanca_LIBRARIES})
