if (UNIX)
  add_definitions(-Wno-sign-compare -Wno-enum-compare)
endif()

add_executable(blackjackserver
  BlackJack_Server.cpp
  Dealer.cpp
  stdafx.cpp
  Table.cpp
  )

target_link_libraries(blackjackserver cpprest)

if(MSVC)
  get_target_property(_srcs blackjackserver SOURCES)
  if(NOT CMAKE_GENERATOR MATCHES "Visual Studio .*")
    set_property(SOURCE stdafx.cpp APPEND PROPERTY OBJECT_OUTPUTS "${CMAKE_CURRENT_BINARY_DIR}/stdafx.pch")
    set_property(SOURCE ${_srcs} APPEND PROPERTY OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/stdafx.pch")
  endif()
  set_source_files_properties(stdafx.cpp PROPERTIES COMPILE_FLAGS "/Ycstdafx.h")
  target_sources(blackjackserver PRIVATE stdafx.cpp)
  target_compile_options(blackjackserver PRIVATE /Yustdafx.h /Zm200)
endif()
