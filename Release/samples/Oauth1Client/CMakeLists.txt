
add_executable(oauth1client
  Oauth1Client.cpp
  stdafx.cpp
  )

target_link_libraries(oauth1client ${Casablanca_LIBRARIES})
