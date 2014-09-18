include_directories(include)

if(UNIX)
  set(CXX_FLAGS "${CXX_FLAGS} -include ../../../include/compat/linux_compat.h")
elseif(WIN32)
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
