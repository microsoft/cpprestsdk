if (UNIX)
  add_definitions(-Wno-sign-compare -Wno-enum-compare)
endif()

add_executable(blackjackserver
  BlackJack_Server.cpp
  Dealer.cpp
  Table.cpp
  )

target_link_libraries(blackjackserver cpprest)
if(MINGW)
  target_link_libraries(blackjackserver ws2_32 wsock32)
endif()

configure_pch(blackjackserver stdafx.h stdafx.cpp /Zm120)
