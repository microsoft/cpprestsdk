include_directories(include)

if(WIN32)
  add_definitions(-DCOMMONUTILITIES_EXPORTS)
endif()

add_library(${LIB}common_utilities
  os_utilities.cpp
  stdafx.cpp
  )

target_link_libraries(${LIB}common_utilities
  ${Casablanca_LIBRARY}
  ${LIB}unittestpp
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  )
