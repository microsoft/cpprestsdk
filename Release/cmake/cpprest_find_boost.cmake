function(cpprest_find_boost)
  if(TARGET cpprestsdk_boost_internal)
    return()
  endif()

  if(IOS)
    set(IOS_SOURCE_DIR "${PROJECT_SOURCE_DIR}/../Build_iOS")
    set(Boost_LIBRARIES "${IOS_SOURCE_DIR}/boost.framework/boost" CACHE INTERNAL "")
    set(Boost_INCLUDE_DIR "${IOS_SOURCE_DIR}/boost.framework/Headers" CACHE INTERNAL "")
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

  add_library(cpprestsdk_boost_internal INTERFACE)
  if(NOT TARGET Boost::boost)
    target_include_directories(cpprestsdk_boost_internal INTERFACE "$<BUILD_INTERFACE:${Boost_INCLUDE_DIR}>")
    target_link_libraries(cpprestsdk_boost_internal INTERFACE "$<BUILD_INTERFACE:${Boost_LIBRARIES}>")
  else()
    if(ANDROID)
      target_link_libraries(cpprestsdk_boost_internal INTERFACE
        Boost::boost
        Boost::random
        Boost::system
        Boost::thread
        Boost::filesystem
        Boost::chrono
        Boost::atomic
      )
    elseif(UNIX)
      target_link_libraries(cpprestsdk_boost_internal INTERFACE
        Boost::boost
        Boost::random
        Boost::system
        Boost::thread
        Boost::filesystem
        Boost::chrono
        Boost::atomic
        Boost::date_time
        Boost::regex
      )
    else()
      target_link_libraries(cpprestsdk_boost_internal INTERFACE
        Boost::boost
        Boost::system
        Boost::date_time
        Boost::regex
      )
    endif()
  endif()
endfunction()
