include_directories(include)

if(WIN32)
  add_definitions(-DCOMMONUTILITIES_EXPORTS)
endif()

add_library(common_utilities
  os_utilities.cpp
  stdafx.cpp
  )

target_link_libraries(common_utilities
  ${Casablanca_LIBRARY}
  unittestpp
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  )
