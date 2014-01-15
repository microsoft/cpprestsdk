include_directories(include)

if(UNIX)
  set(CXX_FLAGS "${CXX_FLAGS} -include ../../../include/compat/linux_compat.h")
endif()

add_library(${LIB}common_utilities
  os_utilities.cpp
  stdafx.cpp
  )
  
target_link_libraries(${LIB}common_utilities
  ${LIB}casablanca
  ${LIB}unittestpp
  ${BOOST_SYSTEM_LIBRARY}
  ${BOOST_THREAD_LIBRARY}
  )
