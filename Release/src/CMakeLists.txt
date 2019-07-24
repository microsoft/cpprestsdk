cmake_policy(SET CMP0022 NEW)

file(GLOB HEADERS_CPPREST "../include/cpprest/*.h" "../include/cpprest/*.hpp" "../include/cpprest/*.dat")
file(GLOB HEADERS_PPLX "../include/pplx/*.h" "../include/pplx/*.hpp")
file(GLOB HEADERS_DETAILS "../include/cpprest/details/*.h" "../include/cpprest/details/*.hpp" "../include/cpprest/details/*.dat" "../include/pplx/*.hpp" "../include/pplx/*.dat")
source_group("Header Files\\cpprest" FILES ${HEADERS_CPPREST})
source_group("Header Files\\pplx" FILES ${HEADERS_PPLX})
source_group("Header Files\\cpprest\\details" FILES ${HEADERS_DETAILS})

file(GLOB HEADER_PPLX_THREADPOOL "../include/pplx/threadpool.h")
list(REMOVE_ITEM HEADERS_PPLX ${HEADER_PPLX_THREADPOOL})

set(SOURCES
  ${HEADERS_CPPREST}
  ${HEADERS_PPLX}
  ${HEADERS_DETAILS}
  pch/stdafx.h
)

if(NOT CPPREST_EXCLUDE_HTTP_NARROW_STRING_WIP)
LIST(APPEND SOURCES
  http/client/http_client.cpp
  http/client/http_client_impl.h
  http/client/http_client_msg.cpp
  http/common/connection_pool_helpers.h
  http/common/http_compression.cpp
  http/common/http_helpers.cpp
  http/common/http_msg.cpp
  http/common/internal_http_helpers.h
  http/listener/http_listener.cpp
  http/listener/http_listener_msg.cpp
  http/listener/http_server_api.cpp
  http/listener/http_server_impl.h
  http/oauth/oauth1.cpp
  http/oauth/oauth2.cpp
  uri/uri.cpp
  uri/uri_builder.cpp
)
endif()

# Targets for properties common to all cpprestsdk libraries
add_library(cpprest_interface_PUBLIC INTERFACE)
target_include_directories(cpprest_interface_PUBLIC
  INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)
add_library(cpprest_interface_PRIVATE INTERFACE)
target_include_directories(cpprest_interface_PRIVATE
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/internal>
)

add_library(cpprest ${SOURCES})
target_include_directories(cpprest
  PUBLIC
    $<INSTALL_INTERFACE:include>
  PRIVATE
    pch
)
target_link_libraries(cpprest PUBLIC cpprest_interface_PUBLIC PRIVATE cpprest_interface_PRIVATE)
set(CPPREST_TARGETS cpprest cpprest_interface_PUBLIC)
if(NOT BUILD_SHARED_LIBS)
  # for installed static libraries the private dependencies must also be
  # "installed". As an interface it should not actually impact install.
  list(APPEND CPPREST_TARGETS cpprest_interface_PRIVATE)
endif()


## Sub-components

# add_sub_component(name sources...)
#  When CPPREST_ISOLATE_COMPONENTS is ON:
#   Creates library ${name} using given sources. Then adds respective
#   object files to cpprest.
#  Otherwise sources are added to cpprest directly.
function(add_sub_component name) # sources...
  set(COMPONENT_SOURCES ${ARGN})

  # cpprest and this new library inherit these properties publicly.
  add_library(${name}_properties_PUBLIC INTERFACE)
  target_link_libraries(${name}_properties_PUBLIC INTERFACE cpprest_interface_PUBLIC)
  # cpprest and this new library inherit these properties privately.
  add_library(${name}_properties_PRIVATE INTERFACE)
  target_link_libraries(${name}_properties_PRIVATE INTERFACE cpprest_interface_PRIVATE)

  if(CPPREST_ISOLATE_COMPONENTS)
    # isolated components are built as OBJECT_LIBRARY so that cpprest library
    # may be self contained (apart from external dependencies) and sources are
    # still only compiled once.
    add_library(${name}_objects OBJECT ${COMPONENT_SOURCES})
    # disable pch for isolated builds to ensure dependencies are explict and
    # to avoid contamination across components by setting include path to
    # reach primitive (ideally empty) stdafx.h.
    target_include_directories(${name}_objects
      PRIVATE
        nopch
    )
	set_target_properties(${name}_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(${name}_objects PRIVATE ${name}_properties_PUBLIC ${name}_properties_PRIVATE)

    # create library of sub-component
    add_library(${name} $<TARGET_OBJECTS:${name}_objects>)
    target_link_libraries(${name} PUBLIC ${name}_properties_PUBLIC PRIVATE ${name}_properties_PRIVATE)

    # connect sources to cpprest (main target)
    target_sources(cpprest PRIVATE $<TARGET_OBJECTS:${name}_objects>)
  else()
    # no isolation, just build sources as part of cpprest
    target_sources(cpprest PRIVATE ${COMPONENT_SOURCES})

    # do create a dummy interface library for sub-components to specify other
    # sub-components in sub_component_link_libraries
    add_library(${name} INTERFACE)
    # also add to target install list in case there is such use
    list(APPEND CPPREST_TARGETS ${name})
  endif()

  target_link_libraries(cpprest PUBLIC ${name}_properties_PUBLIC PRIVATE ${name}_properties_PRIVATE)

  # append public properties (interface) target to install list
  # sub-component libraries (isolation build only) are not installed
  list(APPEND CPPREST_TARGETS ${name}_properties_PUBLIC)
  if(NOT BUILD_SHARED_LIBS)
    # for installed static libraries the private dependencies must also be
    # "installed". Should be interfaces and not actually impact install.
    list(APPEND CPPREST_TARGETS ${name}_properties_PRIVATE)
  endif()
  set(CPPREST_TARGETS ${CPPREST_TARGETS} PARENT_SCOPE)
endfunction()

# sub_component_sources(name sources...)
#  Adds given sources to sub-component library and/or cpprest.
function(sub_component_sources name) # additional sources...
  if(CPPREST_ISOLATE_COMPONENTS)
    target_sources(${name}_objects ${ARGN})
  else()
    # no actual sub-component targets built when not isolated; pass along
    # sources to cpprest
    target_sources(cpprest ${ARGN})
  endif()
endfunction()

# sub_component_link_libraries(name link_dependencies...)
#  Adds given link_dependencies to sub-component library and/or cpprest.
function(sub_component_link_libraries name) # link dependencies...
  if(CPPREST_ISOLATE_COMPONENTS)
    target_link_libraries(${name}_objects ${ARGN})
    target_link_libraries(${name} ${ARGN})
    # remove entries in list that are our own sub-components
    # before passing up to cpprest
    set(ALL_LINK_ARGS ${ARGN})
    foreach(LINK_ARG ${ALL_LINK_ARGS})
      if(TARGET ${LINK_ARG}_properties_PRIVATE)
        list(REMOVE_ITEM ARGN ${LINK_ARG})
      endif()
    endforeach()
  else()
    # no actual sub-component targets built when not isolated
  endif()
  # pass along dependencies to cpprest in all cases
  # (in isolated case only objects are referenced, so dependencies
  # need to be passed up too)
  target_link_libraries(cpprest ${ARGN})
endfunction()


# Utility components
add_sub_component(cpprest_utility_strings
  utilities/string_utils.cpp
)

add_sub_component(cpprest_utility_asyncrt
  utilities/asyncrt_utils.cpp
)
sub_component_link_libraries(cpprest_utility_asyncrt PUBLIC cpprest_utility_strings)

add_sub_component(cpprest_utilities
  utilities/base64.cpp
  utilities/web_utilities.cpp
)
sub_component_link_libraries(cpprest_utilities PUBLIC cpprest_utility_asyncrt cpprest_utility_strings)
if(WIN32 AND NOT WINDOWS_STORE AND NOT WINDOWS_PHONE)
  sub_component_link_libraries(cpprest_utilities PRIVATE
    crypt32.lib
  )
endif()

# Json component
add_sub_component(cpprest_json
  json/json.cpp
  json/json_parsing.cpp
  json/json_serialization.cpp
)
sub_component_link_libraries(cpprest_json PUBLIC cpprest_utility_strings)

# Websockets component
if(CPPREST_WEBSOCKETS_IMPL STREQUAL "none")
  target_compile_definitions(cpprest PUBLIC -DCPPREST_EXCLUDE_WEBSOCKETS=1)
elseif(CPPREST_WEBSOCKETS_IMPL STREQUAL "winrt")
  target_sources(cpprest PRIVATE
    websockets/client/ws_msg.cpp
    websockets/client/ws_client.cpp
    websockets/client/ws_client_impl.h
    websockets/client/ws_client_winrt.cpp
  )
elseif(CPPREST_WEBSOCKETS_IMPL STREQUAL "wspp")
  target_sources(cpprest PRIVATE
    websockets/client/ws_msg.cpp
    websockets/client/ws_client.cpp
    websockets/client/ws_client_impl.h
    websockets/client/ws_client_wspp.cpp
  )
  cpprest_find_websocketpp()
  target_link_libraries(cpprest PRIVATE cpprestsdk_websocketpp_internal)
  cpprest_find_boost()
  cpprest_find_openssl()
  target_link_libraries(cpprest PUBLIC cpprestsdk_boost_internal cpprestsdk_openssl_internal)
else()
  message(FATAL_ERROR "Invalid implementation")
endif()

# Compression component
if(CPPREST_EXCLUDE_COMPRESSION)
  if(NOT CPPREST_EXCLUDE_BROTLI)
    message(FATAL_ERROR "Use of Brotli requires compression to be enabled")
  endif()
  target_compile_definitions(cpprest PRIVATE -DCPPREST_EXCLUDE_COMPRESSION=1)
else()
  cpprest_find_zlib()
  target_link_libraries(cpprest PRIVATE cpprestsdk_zlib_internal)
  if(CPPREST_EXCLUDE_BROTLI)
    target_compile_definitions(cpprest PRIVATE -DCPPREST_EXCLUDE_BROTLI=1)
  else()
    cpprest_find_brotli()
  endif()
endif()

# PPLX component
if(CPPREST_PPLX_IMPL STREQUAL "apple")
  find_library(COREFOUNDATION CoreFoundation "/")
  find_library(SECURITY Security "/")
  add_sub_component(cpprest_pplx pplx/pplxapple.cpp pplx/pplx.cpp pplx/threadpool.cpp ../include/pplx/threadpool.h)
  sub_component_link_libraries(cpprest_pplx PUBLIC ${COREFOUNDATION} ${SECURITY})
  if(CPPREST_INSTALL_HEADERS)
    install(FILES ../include/pplx/threadpool.h DESTINATION include/pplx)
  endif()
elseif(CPPREST_PPLX_IMPL STREQUAL "linux")
  add_sub_component(cpprest_pplx pplx/pplxlinux.cpp pplx/pplx.cpp pplx/threadpool.cpp ../include/pplx/threadpool.h)
  if(CPPREST_INSTALL_HEADERS)
    install(FILES ../include/pplx/threadpool.h DESTINATION include/pplx)
  endif()
elseif(CPPREST_PPLX_IMPL STREQUAL "win")
  add_sub_component(cpprest_pplx pplx/pplxwin.cpp)
  if(CPPREST_WEBSOCKETS_IMPL STREQUAL "wspp")
    sub_component_sources(cpprest_pplx PRIVATE pplx/threadpool.cpp ../include/pplx/threadpool.h)
    if(CPPREST_INSTALL_HEADERS)
      install(FILES ../include/pplx/threadpool.h DESTINATION include/pplx)
    endif()
  endif()
elseif(CPPREST_PPLX_IMPL STREQUAL "winpplx")
  add_sub_component(cpprest_pplx pplx/pplxwin.cpp pplx/pplx.cpp)
  target_compile_definitions(cpprest_pplx_properties_PUBLIC INTERFACE -DCPPREST_FORCE_PPLX=1)
  if(NOT CPPREST_WEBSOCKETS_IMPL STREQUAL "none")
    sub_component_sources(cpprest_pplx PRIVATE pplx/threadpool.cpp ../include/pplx/threadpool.h)
    if(CPPREST_INSTALL_HEADERS)
      install(FILES ../include/pplx/threadpool.h DESTINATION include/pplx)
    endif()
  endif()
elseif(CPPREST_PPLX_IMPL STREQUAL "winrt")
  add_sub_component(cpprest_pplx pplx/pplxwin.cpp)
else()
  message(FATAL_ERROR "Invalid implementation")
endif()
if(NOT CPPREST_PPLX_IMPL STREQUAL "winrt")
  cpprest_find_boost()
  sub_component_link_libraries(cpprest_pplx PUBLIC cpprestsdk_boost_internal)
endif()
sub_component_link_libraries(cpprest_pplx PUBLIC cpprest_utility_asyncrt)

# Http client component
if(NOT CPPREST_EXCLUDE_HTTP_NARROW_STRING_WIP)
if(CPPREST_HTTP_CLIENT_IMPL STREQUAL "asio")
  cpprest_find_boost()
  cpprest_find_openssl()
  target_compile_definitions(cpprest PUBLIC -DCPPREST_FORCE_HTTP_CLIENT_ASIO)
  target_sources(cpprest PRIVATE http/client/http_client_asio.cpp http/client/x509_cert_utilities.cpp)
  target_link_libraries(cpprest PUBLIC cpprestsdk_boost_internal cpprestsdk_openssl_internal)
elseif(CPPREST_HTTP_CLIENT_IMPL STREQUAL "winhttp")
  target_link_libraries(cpprest PRIVATE
    httpapi.lib
    Winhttp.lib
  )
  target_sources(cpprest PRIVATE http/client/http_client_winhttp.cpp)
  if(CPPREST_WEBSOCKETS_IMPL STREQUAL "wspp")
    target_sources(cpprest PRIVATE http/client/x509_cert_utilities.cpp)
  endif()
elseif(CPPREST_HTTP_CLIENT_IMPL STREQUAL "winrt")
  target_sources(cpprest PRIVATE http/client/http_client_winrt.cpp)
else()
  message(FATAL_ERROR "Invalid implementation")
endif()
endif()

# fileio streams component
if(CPPREST_FILEIO_IMPL STREQUAL "win32")
  add_sub_component(cpprest_streams streams/fileio_win32.cpp)
elseif(CPPREST_FILEIO_IMPL STREQUAL "winrt")
  add_sub_component(cpprest_streams streams/fileio_winrt.cpp)
elseif(CPPREST_FILEIO_IMPL STREQUAL "posix")
  add_sub_component(cpprest_streams streams/fileio_posix.cpp)
else()
  message(FATAL_ERROR "Invalid implementation")
endif()
sub_component_link_libraries(cpprest_streams PUBLIC cpprest_utility_asyncrt)

# http listener component
if(CPPREST_HTTP_LISTENER_IMPL STREQUAL "asio")
  cpprest_find_boost()
  cpprest_find_openssl()
  target_compile_definitions(cpprest PUBLIC -DCPPREST_FORCE_HTTP_LISTENER_ASIO)
  target_sources(cpprest PRIVATE http/listener/http_server_asio.cpp)
  target_link_libraries(cpprest PUBLIC cpprestsdk_boost_internal cpprestsdk_openssl_internal)
elseif(CPPREST_HTTP_LISTENER_IMPL STREQUAL "httpsys")
  target_sources(cpprest PRIVATE
    http/listener/http_server_httpsys.cpp
    http/listener/http_server_httpsys.h
  )
elseif(CPPREST_HTTP_LISTENER_IMPL STREQUAL "none")
else()
  message(FATAL_ERROR "Invalid implementation")
endif()

if(MSVC)
  get_target_property(_srcs cpprest SOURCES)

  if(NOT CMAKE_GENERATOR MATCHES "Visual Studio .*")
    set_property(SOURCE pch/stdafx.cpp APPEND PROPERTY OBJECT_OUTPUTS "${CMAKE_CURRENT_BINARY_DIR}/stdafx.pch")
    set_property(SOURCE ${_srcs} APPEND PROPERTY OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/stdafx.pch")
  endif()

  set_source_files_properties(pch/stdafx.cpp PROPERTIES COMPILE_FLAGS "/Ycstdafx.h /Zm120")
  target_sources(cpprest PRIVATE pch/stdafx.cpp)
  target_compile_options(cpprest PRIVATE /Yustdafx.h /Zm120)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  if(WERROR)
    target_compile_options(cpprest_interface_PRIVATE INTERFACE -Werror)
  endif()
  target_compile_options(cpprest_interface_PRIVATE INTERFACE -pedantic ${WARNINGS})
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  if(WERROR)
    target_compile_options(cpprest_interface_PRIVATE INTERFACE /WX ${WARNINGS})
  endif()
else()
  message(FATAL_ERROR "Unknown compiler")
endif()

if(WIN32)
  if (BUILD_SHARED_LIBS)
    target_compile_definitions(cpprest_interface_PRIVATE INTERFACE -D_ASYNCRT_EXPORT -D_PPLX_EXPORT -D_USRDLL)
  else()
    target_compile_definitions(cpprest_interface_PUBLIC INTERFACE -D_NO_ASYNCRTIMP -D_NO_PPLXIMP)
  endif()
elseif(ANDROID)
  target_link_libraries(cpprest_interface_PRIVATE INTERFACE ${ANDROID_STL_FLAGS})
endif()

if (WIN32 AND NOT WINDOWS_STORE AND NOT WINDOWS_PHONE)
  target_link_libraries(cpprest PRIVATE
    bcrypt.lib
  )
elseif(WINDOWS_STORE)
  if(NOT CMAKE_GENERATOR MATCHES "Visual Studio .*")
    target_compile_definitions(cpprest_interface_PRIVATE INTERFACE -DWINAPI_FAMILY=WINAPI_FAMILY_PC_APP)
    get_target_property(LINK_FLAGS cpprest LINK_FLAGS)
    if(NOT LINK_FLAGS)
        set(LINK_FLAGS "")
    endif()
    set(LINK_FLAGS "${LINK_FLAGS} /APPCONTAINER")
    set_target_properties(cpprest PROPERTIES LINK_FLAGS "${LINK_FLAGS}")
  endif()
endif()

set_target_properties(cpprest PROPERTIES OUTPUT_NAME "cpprest${CPPREST_ABI_TAG}")
if(WIN32)
elseif(ANDROID)
  # Do not use SOVERSION on android. It is completely unsupported (and causes problems).
  # Perhaps revisit in the future? (NDK r9d, 8/7/14)
else()
  set_target_properties(cpprest PROPERTIES
    SOVERSION ${CPPREST_VERSION_MAJOR}.${CPPREST_VERSION_MINOR})
endif()

if(CPPREST_INSTALL_HEADERS)
  install(FILES ${HEADERS_CPPREST} DESTINATION include/cpprest)
  install(FILES ${HEADERS_PPLX} DESTINATION include/pplx)
  install(FILES ${HEADERS_DETAILS} DESTINATION include/cpprest/details)
endif()

if(CPPREST_INSTALL)
  set(CPPREST_USES_BOOST OFF)
  set(CPPREST_USES_ZLIB OFF)
  set(CPPREST_USES_BROTLI OFF)
  set(CPPREST_USES_OPENSSL OFF)

  if(TARGET cpprestsdk_boost_internal)
    list(APPEND CPPREST_TARGETS cpprestsdk_boost_internal)
    set(CPPREST_USES_BOOST ON)
  endif()
  if(TARGET cpprestsdk_zlib_internal)
    list(APPEND CPPREST_TARGETS cpprestsdk_zlib_internal)
    set(CPPREST_USES_ZLIB ON)
  endif()
  if(TARGET cpprestsdk_brotli_internal)
    list(APPEND CPPREST_TARGETS cpprestsdk_brotli_internal)
    set(CPPREST_USES_BROTLI ON)
  endif()
  if(TARGET cpprestsdk_openssl_internal)
    list(APPEND CPPREST_TARGETS cpprestsdk_openssl_internal)
    set(CPPREST_USES_OPENSSL ON)
  endif()
  if(TARGET cpprestsdk_websocketpp_internal)
    list(APPEND CPPREST_TARGETS cpprestsdk_websocketpp_internal)
  endif()
  install(
    TARGETS ${CPPREST_TARGETS}
    EXPORT cpprestsdk-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

  configure_file(../cmake/cpprestsdk-config.in.cmake "${CMAKE_CURRENT_BINARY_DIR}/cpprestsdk-config.cmake" @ONLY)

  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/cpprestsdk-config.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/${CPPREST_EXPORT_DIR}
  )
  install(
    EXPORT cpprestsdk-targets
    FILE cpprestsdk-targets.cmake
    NAMESPACE cpprestsdk::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/${CPPREST_EXPORT_DIR}
  )
endif()
