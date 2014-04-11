
add_executable(oauth2client
  Oauth2Client.cpp
  stdafx.cpp
  )

target_link_libraries(oauth2client ${Casablanca_LIBRARIES})
