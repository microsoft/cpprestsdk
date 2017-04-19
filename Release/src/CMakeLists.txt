cmake_policy(SET CMP0022 NEW)

file(GLOB HEADERS_CPPREST "../include/cpprest/*.h" "../include/cpprest/*.hpp" "../include/cpprest/*.dat")
file(GLOB HEADERS_PPLX "../include/pplx/*.h" "../include/pplx/*.hpp")
file(GLOB HEADERS_DETAILS "../include/cpprest/details/*.h" "../include/cpprest/details/*.hpp" "../include/cpprest/details/*.dat" "../include/pplx/*.hpp" "../include/pplx/*.dat")
source_group("Header Files\\cpprest" FILES ${HEADERS_CPPREST})
source_group("Header Files\\pplx" FILES ${HEADERS_PPLX})
source_group("Header Files\\cpprest\\details" FILES ${HEADERS_DETAILS})

set(SOURCES
  ${HEADERS_CPPREST}
  ${HEADERS_PPLX}
  ${HEADERS_DETAILS}
  pch/stdafx.h
  http/client/http_client.cpp
  http/client/http_client_msg.cpp
  http/client/http_client_impl.h
  http/client/x509_cert_utilities.cpp
  http/common/internal_http_helpers.h
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
)

## Sub-component sources
# Websockets component
if(CPPREST_WEBSOCKETS_IMPL STREQUAL "none")
elseif(CPPREST_WEBSOCKETS_IMPL STREQUAL "winrt")
  list(APPEND SOURCES
    websockets/client/ws_msg.cpp
    websockets/client/ws_client.cpp
    websockets/client/ws_client_winrt.cpp
  )
elseif(CPPREST_WEBSOCKETS_IMPL STREQUAL "wspp")
  list(APPEND SOURCES
    websockets/client/ws_msg.cpp
    websockets/client/ws_client.cpp
    websockets/client/ws_client_wspp.cpp
  )
endif()

# Compression component
# No sources to add

# PPLX component
if(CPPREST_PPLX_IMPL STREQUAL "apple")
  list(APPEND SOURCES
    pplx/threadpool.cpp
    pplx/pplxapple.cpp
  )
elseif(CPPREST_PPLX_IMPL STREQUAL "linux")
  list(APPEND SOURCES
    pplx/threadpool.cpp
    pplx/pplxlinux.cpp
  )
elseif(CPPREST_PPLX_IMPL STREQUAL "win")
  list(APPEND SOURCES
    pplx/threadpool.cpp
    pplx/pplxwin.cpp
  )
endif()

# Http client component
if(CPPREST_HTTP_CLIENT_IMPL STREQUAL "asio")
  list(APPEND SOURCES http/client/http_client_asio.cpp)
elseif(CPPREST_HTTP_CLIENT_IMPL STREQUAL "winhttp")
  list(APPEND SOURCES http/client/http_client_winhttp.cpp)
elseif(CPPREST_HTTP_CLIENT_IMPL STREQUAL "winrt")
  list(APPEND SOURCES http/client/http_client_winrt.cpp)
endif()

# fileio streams component
if(CPPREST_FILEIO_IMPL STREQUAL "win32")
  list(APPEND SOURCES streams/fileio_win32.cpp)
elseif(CPPREST_FILEIO_IMPL STREQUAL "winrt")
  list(APPEND SOURCES streams/fileio_winrt.cpp)
elseif(CPPREST_FILEIO_IMPL STREQUAL "posix")
  list(APPEND SOURCES streams/fileio_posix.cpp)
endif()

# http listener component
if(CPPREST_HTTP_LISTENER_IMPL STREQUAL "asio")
  list(APPEND SOURCES http/listener/http_server_asio.cpp)
elseif(CPPREST_HTTP_LISTENER_IMPL STREQUAL "httpsys")
  list(APPEND SOURCES http/listener/http_server_httpsys.cpp)
endif()

if(MSVC)
  set_source_files_properties(pch/stdafx.cpp PROPERTIES COMPILE_FLAGS "/Ycstdafx.h")

  if (NOT ${CMAKE_GENERATOR} MATCHES "Visual Studio .*")
    set_property(SOURCE pch/stdafx.cpp APPEND PROPERTY OBJECT_OUTPUTS "${CMAKE_CURRENT_BINARY_DIR}/stdafx.pch")
    set_property(SOURCE ${SOURCES} APPEND PROPERTY OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/stdafx.pch")
  endif()

  add_library(cpprest ${SOURCES} pch/stdafx.cpp)
  target_compile_options(cpprest PRIVATE /Yustdafx.h /Zm200)
else()
  add_library(cpprest ${SOURCES})
endif()

target_include_directories(cpprest
  PUBLIC
    $<INSTALL_INTERFACE:include> $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  PRIVATE
    pch
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  if(WERROR)
    target_compile_options(cpprest PRIVATE -Werror)
  endif()
  target_compile_options(cpprest PRIVATE -pedantic)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  if(WERROR)
    target_compile_options(cpprest PRIVATE /WX)
  endif()
endif()

if (BUILD_SHARED_LIBS AND WIN32)
  target_compile_definitions(cpprest PRIVATE -D_ASYNCRT_EXPORT -D_PPLX_EXPORT -D_USRDLL)
endif()

target_compile_options(cpprest PRIVATE ${WARNINGS})

set(CPPREST_USES_BOOST OFF)
set(CPPREST_USES_OPENSSL OFF)
set(CPPREST_USES_ZLIB OFF)
set(CPPREST_USES_WEBSOCKETPP OFF)

if (WIN32 AND NOT WINDOWS_STORE AND NOT WINDOWS_PHONE)
  target_link_libraries(cpprest PRIVATE
    bcrypt.lib
    crypt32.lib
  )
endif()

# Websockets component
if(CPPREST_WEBSOCKETS_IMPL STREQUAL "none")
  target_compile_definitions(cpprest PUBLIC -DCPPREST_EXCLUDE_WEBSOCKETS=1)
elseif(CPPREST_WEBSOCKETS_IMPL STREQUAL "winrt")
elseif(CPPREST_WEBSOCKETS_IMPL STREQUAL "wspp")
  set(CPPREST_USES_WEBSOCKETPP ON)
  set(CPPREST_USES_BOOST ON)
  set(CPPREST_USES_OPENSSL ON)
  set(CPPREST_USES_ZLIB ON)
else()
  message(FATAL_ERROR "Invalid implementation")
endif()

# Compression component
if(CPPREST_EXCLUDE_COMPRESSION)
  target_compile_definitions(cpprest PUBLIC -DCPPREST_EXCLUDE_COMPRESSION=1)
else()
  set(CPPREST_USES_ZLIB ON)
endif()

# PPLX component
if(CPPREST_PPLX_IMPL STREQUAL "apple")
  find_library(COREFOUNDATION CoreFoundation "/")
  find_library(SECURITY Security "/")
  target_link_libraries(cpprest PRIVATE ${COREFOUNDATION} ${SECURITY})
elseif(CPPREST_PPLX_IMPL STREQUAL "linux")
elseif(CPPREST_PPLX_IMPL STREQUAL "win")
else()
  message(FATAL_ERROR "Invalid implementation")
endif()

# Http client component
if(CPPREST_HTTP_CLIENT_IMPL STREQUAL "asio")
  set(CPPREST_USES_BOOST ON)
  set(CPPREST_USES_OPENSSL ON)
  target_compile_definitions(cpprest PUBLIC -DCPPREST_FORCE_HTTP_CLIENT_ASIO)
elseif(CPPREST_HTTP_CLIENT_IMPL STREQUAL "winhttp")
  target_link_libraries(cpprest PRIVATE
    httpapi.lib
    Winhttp.lib
  )
elseif(CPPREST_HTTP_CLIENT_IMPL STREQUAL "winrt")
else()
  message(FATAL_ERROR "Invalid implementation")
endif()

# fileio streams component
if(CPPREST_FILEIO_IMPL STREQUAL "win32")
elseif(CPPREST_FILEIO_IMPL STREQUAL "winrt")
elseif(CPPREST_FILEIO_IMPL STREQUAL "posix")
else()
  message(FATAL_ERROR "Invalid implementation")
endif()

# http listener component
if(CPPREST_HTTP_LISTENER_IMPL STREQUAL "asio")
  set(CPPREST_USES_BOOST ON)
  set(CPPREST_USES_OPENSSL ON)
  target_compile_definitions(cpprest PUBLIC -DCPPREST_FORCE_HTTP_LISTENER_ASIO)
elseif(CPPREST_HTTP_LISTENER_IMPL STREQUAL "httpsys")
elseif(CPPREST_HTTP_LISTENER_IMPL STREQUAL "none")
else()
  message(FATAL_ERROR "Invalid implementation")
endif()

# Add any libraries that were used.
if(CPPREST_USES_WEBSOCKETPP)
  cpprest_find_websocketpp()
  target_include_directories(cpprest SYSTEM PRIVATE ${WEBSOCKETPP_INCLUDE_DIR})
endif()
if(CPPREST_USES_BOOST)
  cpprest_find_boost()
  target_link_libraries(cpprest PUBLIC ${Boost_LIBRARIES}) 
  target_include_directories(cpprest SYSTEM PUBLIC ${Boost_INCLUDE_DIR})
endif()
if(CPPREST_USES_OPENSSL)
  cpprest_find_openssl()
  target_link_libraries(cpprest PUBLIC ${OPENSSL_LIBRARIES})
  target_include_directories(cpprest SYSTEM PUBLIC ${OPENSSL_INCLUDE_DIR})
  if (NOT _SSL_LEAK_SUPPRESS_AVAILABLE)
    # libressl doesn't ship with the cleanup method being used in ws_client_wspp
    target_compile_definitions(cpprest PRIVATE -DCPPREST_NO_SSL_LEAK_SUPPRESS)
  endif()
endif()
if(CPPREST_USES_ZLIB)
  cpprest_find_zlib()
  target_link_libraries(cpprest PUBLIC ${ZLIB_LIBRARIES})
  target_include_directories(cpprest SYSTEM PUBLIC ${ZLIB_INCLUDE_DIR})
endif()
if(ANDROID)
  target_link_libraries(cpprest PRIVATE ${ANDROID_STL_FLAGS})
endif()

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
endif()

install(
  TARGETS cpprest
  EXPORT ${CPPREST_EXPORT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

if(CPPREST_INSTALL_HEADERS)
  install(FILES ${HEADERS_CPPREST} DESTINATION include/cpprest)
  install(FILES ${HEADERS_PPLX} DESTINATION include/pplx)
  install(FILES ${HEADERS_DETAILS} DESTINATION include/cpprest/details)
endif()

install(EXPORT ${CPPREST_EXPORT_NAME} DESTINATION ${CPPREST_EXPORT_DIR})
