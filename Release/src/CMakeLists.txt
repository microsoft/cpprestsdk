include_directories(../include pch ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})

# THE ORDER OF FILES IS VERY /VERY/ IMPORTANT
if(UNIX)
  set(SOURCES
    streams/fileio_posix.cpp
    json/json.cpp
    json/json_parsing.cpp
    json/json_serialization.cpp
    utilities/asyncrt_utils.cpp
    utilities/web_utilities.cpp
    pplx/pplx.cpp
    pplx/threadpool.cpp
    uri/uri.cpp
    uri/uri_builder.cpp
    uri/uri_parser.cpp
    http/common/http_msg.cpp
    http/common/http_helpers.cpp
    http/client/http_client_msg.cpp
    http/client/http_client_asio.cpp
    http/client/x509_cert_utilities.cpp
    http/listener/http_server_asio.cpp
    http/listener/http_listener.cpp
    http/listener/http_listener_msg.cpp
    http/listener/http_server_api.cpp
    http/oauth/oauth1.cpp
    http/oauth/oauth2.cpp
    websockets/client/ws_msg.cpp
    websockets/client/ws_client.cpp
    websockets/client/ws_client_wspp.cpp
    utilities/base64.cpp
    )
  if(APPLE)
    set(SOURCES ${SOURCES} 
      pplx/pplxapple.cpp
      http/client/x509_cert_utilities_apple.cpp
      )
    find_library(COREFOUNDATION CoreFoundation "/")
    find_library(SECURITY Security "/")
    set(EXTRALINKS ${COREFOUNDATION} ${SECURITY})
  elseif(ANDROID)
    set(SOURCES $(SOURCES) http/client/x509_cert_utilities_android.cpp)
  else()
    set(SOURCES ${SOURCES} pplx/pplxlinux.cpp)
  endif()
endif()

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNINGS} -Werror -pedantic")

add_library(${Casablanca_LIBRARY} ${SOURCES})

target_link_libraries(${Casablanca_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT}
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  ${Boost_ATOMIC_LIBRARY}
  ${Boost_CHRONO_LIBRARY}
  ${Boost_LOCALE_LIBRARY}
  ${Boost_RANDOM_LIBRARY}
  ${EXTRALINKS}
  ${Boost_FRAMEWORK}
  ${OPENSSL_LIBRARIES}
  ${COREFOUNDATION}
  ${ANDROID_STL_FLAGS}
  )

# Portions specific to cpprest binary versioning.
set (CPPREST_VERSION_MAJOR 2)
set (CPPREST_VERSION_MINOR 4)
set (CPPREST_VERSION_REVISION 0)

if(WIN32)
  set_target_properties(${Casablanca_LIBRARY} PROPERTIES
    OUTPUT_NAME "${Casablanca_LIBRARY}_${CPPREST_VERSION_MAJOR}_${CPPREST_VERSION_MINOR}")
elseif(ANDROID)
  # Do not use SOVERSION on android. It is completely unsupported (and causes problems).
  # Perhaps revisit in the future? (NDK r9d, 8/7/14)
else()
  set_target_properties(${Casablanca_LIBRARY} PROPERTIES
    SOVERSION ${CPPREST_VERSION_MAJOR}.${CPPREST_VERSION_MINOR})

  install(
    TARGETS ${Casablanca_LIBRARY}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    )
endif()
