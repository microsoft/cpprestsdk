include(CMakeFindDependencyMacro)
if(@CPPREST_USES_ZLIB@)
  find_dependency(ZLIB)
endif()

if(@CPPREST_USES_BROTLI@)
  find_dependency(unofficial-brotli)
endif()

if(@CPPREST_USES_OPENSSL@)
  find_dependency(OpenSSL)
endif()

if(@CPPREST_USES_WINHTTPPAL@)
  find_dependency(WINHTTPPAL)
endif()

if(@CPPREST_USES_BOOST@ AND OFF)
  if(UNIX)
    find_dependency(Boost COMPONENTS random system thread filesystem chrono atomic date_time regex)
  else()
    find_dependency(Boost COMPONENTS system date_time regex)
  endif()
endif()
include("${CMAKE_CURRENT_LIST_DIR}/cpprestsdk-targets.cmake")
