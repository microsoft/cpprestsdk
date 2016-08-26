include_directories(../include pch)
if (NOT CPPREST_EXCLUDE_WEBSOCKETS)
  include_directories(SYSTEM ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})
endif()

add_definitions(${WARNINGS})

file(GLOB SOURCES_CPPREST "../include/cpprest/*.h")
file(GLOB SOURCES_PPLX "../include/pplx/*.h")
file(GLOB SOURCES_DETAILS "../include/cpprest/details/*.h")
source_group("Header Files\\cpprest" FILES ${SOURCES_CPPREST})
source_group("Header Files\\pplx" FILES ${SOURCES_PPLX})
source_group("Header Files\\cpprest\\details" FILES ${SOURCES_DETAILS})
set(SOURCES_COMMON
  ${SOURCES_CPPREST}
  ${SOURCES_PPLX}
  ${SOURCES_DETAILS}
  http/client/http_client.cpp
  http/client/http_client_msg.cpp
  http/client/http_client_impl.h
  http/client/x509_cert_utilities.cpp
  http/common/http_helpers.cpp
  http/common/http_msg.cpp
  http/listener/http_listener.cpp
  http/listener/http_listener_msg.cpp
  http/listener/http_server_api.cpp
  http/oauth/oauth1.cpp
  http/oauth/oauth2.cpp
  json/json.cpp
  json/json_parsing.cpp
  json/json_serialization.cpp
  pplx/pplx.cpp
  uri/uri.cpp
  uri/uri_builder.cpp
  uri/uri_parser.cpp
  utilities/asyncrt_utils.cpp
  utilities/base64.cpp
  utilities/web_utilities.cpp
  websockets/client/ws_msg.cpp
  websockets/client/ws_client.cpp
)

# THE ORDER OF FILES IS VERY /VERY/ IMPORTANT
if(UNIX)
  set(SOURCES
    ${SOURCES_COMMON}
    streams/fileio_posix.cpp
    pplx/threadpool.cpp
    http/client/http_client_asio.cpp
    http/listener/http_server_asio.cpp
  )
  if (NOT CPPREST_EXCLUDE_WEBSOCKETS)
    list(APPEND SOURCES websockets/client/ws_client_wspp.cpp)
  endif()
  if(APPLE)
    list(APPEND SOURCES
      pplx/pplxapple.cpp
    )
    find_library(COREFOUNDATION CoreFoundation "/")
    find_library(SECURITY Security "/")
    set(EXTRALINKS ${COREFOUNDATION} ${SECURITY})
  elseif(ANDROID)
    list(APPEND SOURCES
      pplx/pplxlinux.cpp
    )
  else()
    list(APPEND SOURCES pplx/pplxlinux.cpp)
  endif()

  if(WERROR)
      add_compile_options(-Werror)
  endif()
  add_compile_options(-pedantic)
elseif(WIN32)
  set(SOURCES ${SOURCES_COMMON}
    pplx/pplxwin.cpp
  )
  if (WINDOWS_PHONE OR WINDOWS_STORE)
    list(APPEND SOURCES
      http/client/http_client_winrt.cpp
      streams/fileio_winrt.cpp
      websockets/client/ws_client_winrt.cpp
    )
  else()
    list(APPEND SOURCES
      http/client/http_client_winhttp.cpp
      http/listener/http_server_httpsys.cpp
      streams/fileio_win32.cpp
    )
    if (NOT CPPREST_EXCLUDE_WEBSOCKETS)
      list(APPEND SOURCES websockets/client/ws_client_wspp.cpp)
    endif()
    set(EXTRALINKS
      bcrypt.lib
      crypt32.lib
      httpapi.lib
      Winhttp.lib
    )
  endif()
  add_compile_options(/Yustdafx.h /Zm200)
  set_source_files_properties(pch/stdafx.cpp PROPERTIES COMPILE_FLAGS "/Ycstdafx.h")

  if (NOT ${CMAKE_GENERATOR} MATCHES "Visual Studio .*")
    set_property(SOURCE pch/stdafx.cpp APPEND PROPERTY OBJECT_OUTPUTS "${CMAKE_CURRENT_BINARY_DIR}/stdafx.pch")
    set_property(SOURCE ${SOURCES} APPEND PROPERTY OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/stdafx.pch")
  endif()

  list(APPEND SOURCES pch/stdafx.cpp pch/stdafx.h)

  if (BUILD_SHARED_LIBS)
    add_definitions(-D_ASYNCRT_EXPORT -D_PPLX_EXPORT -D_USRDLL)
  endif()
endif()

add_library(cpprest ${SOURCES})

target_link_libraries(cpprest
  ${CMAKE_THREAD_LIBS_INIT}
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  ${Boost_ATOMIC_LIBRARY}
  ${Boost_CHRONO_LIBRARY}
  ${Boost_RANDOM_LIBRARY}
  ${Boost_REGEX_LIBRARY}
  ${Boost_DATE_TIME_LIBRARY}
  ${EXTRALINKS}
  ${Boost_FRAMEWORK}
  ${OPENSSL_LIBRARIES}
  ${COREFOUNDATION}
  ${ANDROID_STL_FLAGS}
  )

# Portions specific to cpprest binary versioning.
set (CPPREST_VERSION_MAJOR 2)
set (CPPREST_VERSION_MINOR 9)
set (CPPREST_VERSION_REVISION 0)

if(WIN32)
  set_target_properties(cpprest PROPERTIES
    OUTPUT_NAME "cpprest_${CPPREST_VERSION_MAJOR}_${CPPREST_VERSION_MINOR}")
elseif(ANDROID)
  # Do not use SOVERSION on android. It is completely unsupported (and causes problems).
  # Perhaps revisit in the future? (NDK r9d, 8/7/14)
else()
  set_target_properties(cpprest PROPERTIES
    SOVERSION ${CPPREST_VERSION_MAJOR}.${CPPREST_VERSION_MINOR})

  install(
    TARGETS cpprest
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    )
endif()
