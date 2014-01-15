add_subdirectory(SearchFile)
add_subdirectory(BingRequest)
add_subdirectory(BlackJack)

add_custom_target(samples
  DEPENDS SearchFile BingRequest blackjackclient blackjackserver
  )