function(cpprest_find_boost)
  if(Boost_FOUND)
    return()
  endif()

  if(IOS)
    set(IOS_SOURCE_DIR "${PROJECT_SOURCE_DIR}/../Build_iOS")
    set(Boost_FOUND 1 CACHE INTERNAL "")
    set(Boost_FRAMEWORK "-F ${IOS_SOURCE_DIR} -framework boost" CACHE INTERNAL "")
    set(Boost_INCLUDE_DIR "$<BUILD_INTERFACE:${IOS_SOURCE_DIR}/boost.framework/Headers>" CACHE INTERNAL "")
  elseif(ANDROID)
    set(Boost_COMPILER "-clang")
    if(ARM)
      set(BOOST_ROOT "${CMAKE_BINARY_DIR}/../Boost-for-Android/build" CACHE INTERNAL "")
      set(BOOST_LIBRARYDIR "${CMAKE_BINARY_DIR}/../Boost-for-Android/build/lib" CACHE INTERNAL "")
    else()
      set(BOOST_ROOT "${CMAKE_BINARY_DIR}/../Boost-for-Android-x86/build" CACHE INTERNAL "")
      set(BOOST_LIBRARYDIR "${CMAKE_BINARY_DIR}/../Boost-for-Android-x86/build/lib" CACHE INTERNAL "")
    endif()
    find_host_package(Boost 1.55 EXACT REQUIRED COMPONENTS random system thread filesystem chrono atomic)
  elseif(UNIX)
    find_package(Boost REQUIRED COMPONENTS random system thread filesystem chrono atomic date_time regex)
  else()
    find_package(Boost REQUIRED COMPONENTS system date_time regex)
  endif()

  set(Boost_FOUND 1 CACHE INTERNAL "")
  set(Boost_INCLUDE_DIR ${Boost_INCLUDE_DIR} CACHE INTERNAL "")
  set(Boost_LIBRARIES
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_ATOMIC_LIBRARY}
    ${Boost_CHRONO_LIBRARY}
    ${Boost_RANDOM_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${Boost_DATE_TIME_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${BOOST_FRAMEWORK}
    CACHE INTERNAL "")
 endfunction()