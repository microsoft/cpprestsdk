set(CMAKE_LEGACY_CYGWIN_WIN32 0)
cmake_minimum_required(VERSION 2.6)
project(cpprest)

enable_testing()

set(WARNINGS)
set(ANDROID_STL_FLAGS)

option(WERROR "Threat Warnings as Errors" ON)
option(BUILD_TESTS "Build tests." ON)
option(CPPREST_EXCLUDE_WEBSOCKETS "Exclude websockets functionality." OFF)

# Platform (not compiler) specific settings
if(IOS)
  set(IOS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Build_iOS")
  set(Boost_FRAMEWORK "-F ${IOS_SOURCE_DIR} -framework boost")
  set(Boost_INCLUDE_DIR "${IOS_SOURCE_DIR}/boost.framework/Headers")

  set(OPENSSL_FOUND 1)
  set(OPENSSL_INCLUDE_DIR "${IOS_SOURCE_DIR}/openssl/include")
  set(OPENSSL_LIBRARIES
    "${IOS_SOURCE_DIR}/openssl/lib/libcrypto.a"
    "${IOS_SOURCE_DIR}/openssl/lib/libssl.a"
    )

  # The cxx_flags must be reset here, because the ios-cmake toolchain file unfortunately sets "-headerpad_max_install_names" which is not a valid clang flag.
  set(CMAKE_CXX_FLAGS "-fvisibility=hidden -fvisibility-inlines-hidden")

  set(BUILD_SHARED_LIBS OFF)
  set(BUILD_SAMPLES OFF)
elseif(ANDROID)
  set(Boost_COMPILER "-clang")
  set(Boost_USE_STATIC_LIBS ON)
  if(ARM)
    set(BOOST_ROOT "${CMAKE_BINARY_DIR}/../Boost-for-Android/build")
    set(BOOST_LIBRARYDIR "${CMAKE_BINARY_DIR}/../Boost-for-Android/build/lib")
  else()
    set(BOOST_ROOT "${CMAKE_BINARY_DIR}/../Boost-for-Android-x86/build")
    set(BOOST_LIBRARYDIR "${CMAKE_BINARY_DIR}/../Boost-for-Android-x86/build/lib")
  endif()
  find_host_package(Boost 1.55 EXACT REQUIRED COMPONENTS random system thread filesystem chrono atomic)

  set(OPENSSL_FOUND 1)
  if(ARM)
    set(OPENSSL_INCLUDE_DIR "${CMAKE_BINARY_DIR}/../openssl/armeabi-v7a/include")
    set(OPENSSL_LIBRARIES
      "${CMAKE_BINARY_DIR}/../openssl/armeabi-v7a/lib/libssl.a"
      "${CMAKE_BINARY_DIR}/../openssl/armeabi-v7a/lib/libcrypto.a"
      )
  else()
    set(OPENSSL_INCLUDE_DIR "${CMAKE_BINARY_DIR}/../openssl/x86/include")
    set(OPENSSL_LIBRARIES
      "${CMAKE_BINARY_DIR}/../openssl/x86/lib/libssl.a"
      "${CMAKE_BINARY_DIR}/../openssl/x86/lib/libcrypto.a"
      )
  endif()

  if(ARM)
    set(LIBCXX_STL "${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/4.8/libs/armeabi-v7a/thumb/libgnustl_static.a")
  else()
    set(LIBCXX_STL "${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/4.8/libs/x86/libgnustl_static.a")
  endif()
  # These are used in the shared library case
  set(ANDROID_STL_FLAGS
    ${LIBCXX_STL}
    atomic
    dl
    gcc
    c
    m
    -nodefaultlibs
    )

  option(BUILD_SHARED_LIBS "Build shared Libraries." OFF)
  set(BUILD_SAMPLES OFF)
elseif(UNIX) # This includes OSX
  find_package(Boost 1.54 REQUIRED COMPONENTS random chrono system thread regex filesystem)
  find_package(Threads REQUIRED)
  if(APPLE AND NOT OPENSSL_ROOT_DIR)
    # Prefer a homebrew version of OpenSSL over the one in /usr/lib
    file(GLOB OPENSSL_ROOT_DIR /usr/local/Cellar/openssl/*)
    # Prefer the latest (make the latest one first)
    list(REVERSE OPENSSL_ROOT_DIR)
  endif()
  # This should prevent linking against the system provided 0.9.8y
  set(_OPENSSL_VERSION "")
  find_package(OpenSSL 1.0.0 REQUIRED)

  option(BUILD_SHARED_LIBS "Build shared Libraries." ON)
  option(BUILD_SAMPLES "Build samples." ON)
  option(CASA_INSTALL_HEADERS "Install header files." ON)
  if(CASA_INSTALL_HEADERS)
    file(GLOB CASA_HEADERS_CPPREST include/cpprest/*.hpp include/cpprest/*.h include/cpprest/*.dat)
    install(FILES ${CASA_HEADERS_CPPREST} DESTINATION include/cpprest)
    file(GLOB CASA_HEADERS_PPLX include/pplx/*.hpp include/pplx/*.h)
    install(FILES ${CASA_HEADERS_PPLX} DESTINATION include/pplx)
    file(GLOB CASA_HEADERS_DETAILS include/cpprest/details/*.hpp include/cpprest/details/*.h include/cpprest/details/*.dat)
    install(FILES ${CASA_HEADERS_DETAILS} DESTINATION include/cpprest/details)
  endif()
elseif(WIN32)
  option(BUILD_SHARED_LIBS "Build shared Libraries." ON)
  option(BUILD_SAMPLES "Build samples." ON)
  option(Boost_USE_STATIC_LIBS ON)

  add_definitions(-DUNICODE -D_UNICODE)

  if(NOT BUILD_SHARED_LIBS)
    # This causes cmake to not link the test libraries separately, but instead hold onto their object files.
    set(TEST_LIBRARY_TARGET_TYPE OBJECT)
    set(Casablanca_DEFINITIONS -D_NO_ASYNCRTIMP -D_NO_PPLXIMP CACHE INTERNAL "Definitions for consume casablanca library")
  else()
    set(Casablanca_DEFINITIONS "" CACHE INTERNAL "Definitions for consume casablanca library")
  endif()
  add_definitions(${Casablanca_DEFINITIONS} -D_WINSOCK_DEPRECATED_NO_WARNINGS -DWIN32)

  if (NOT CPPREST_EXCLUDE_WEBSOCKETS)
    set(NUGET_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../packages")
    set(PACKAGE_PATHS)
    list(APPEND PACKAGE_PATHS "${NUGET_PATH}/boost.1.58.0.0/")
    list(APPEND PACKAGE_PATHS "${NUGET_PATH}/boost_system-vc140.1.58.0-vs140rc/")
    list(APPEND PACKAGE_PATHS "${NUGET_PATH}/boost_date_time-vc140.1.58.0-vs140rc/")
    list(APPEND PACKAGE_PATHS "${NUGET_PATH}/boost_regex-vc140.1.58.0-vs140rc/")
    list(APPEND PACKAGE_PATHS "${NUGET_PATH}/openssl.v140.windesktop.msvcstl.static.rt-dyn.x64.1.0.2.1/")
    list(APPEND PACKAGE_PATHS "${NUGET_PATH}/zlib.v140.windesktop.msvcstl.static.rt-dyn.1.2.8.8/")

    if (NOT WINDOWS_STORE AND NOT WINDOWS_PHONE)
      find_library(Boost_SYSTEM_LIBRARY libboost_system-vc140-mt-gd-1_58.lib PATHS ${PACKAGE_PATHS} PATH_SUFFIXES lib/native/address-model-64/lib)
      find_library(Boost_DATE_TIME_LIBRARY libboost_date_time-vc140-mt-gd-1_58.lib PATHS ${PACKAGE_PATHS} PATH_SUFFIXES lib/native/address-model-64/lib)
      find_library(Boost_REGEX_LIBRARY libboost_regex-vc140-mt-gd-1_58.lib PATHS ${PACKAGE_PATHS} PATH_SUFFIXES lib/native/address-model-64/lib)
      set(Boost_LIBRARIES ${Boost_REGEX_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_DATE_TIME_LIBRARY})
  
      find_library(OpenSSL_libeay_LIBRARY libeay32.lib PATHS ${PACKAGE_PATHS} PATH_SUFFIXES lib/native/v140/windesktop/msvcstl/static/rt-dyn/x64/debug)
      find_library(OpenSSL_ssleay_LIBRARY ssleay32.lib PATHS ${PACKAGE_PATHS} PATH_SUFFIXES lib/native/v140/windesktop/msvcstl/static/rt-dyn/x64/debug)
      find_library(ZLIB_LIBRARY zlibstaticd.lib PATHS ${PACKAGE_PATHS} PATH_SUFFIXES lib/native/v140/windesktop/msvcstl/static/rt-dyn/x64/debug)
      set(OPENSSL_LIBRARIES ${OpenSSL_ssleay_LIBRARY} ${OpenSSL_libeay_LIBRARY} ${ZLIB_LIBRARY})

      set(OPENSSL_INCLUDE_DIR "${NUGET_PATH}/openssl.v140.windesktop.msvcstl.static.rt-dyn.x64.1.0.2.1/build/native/include")
    endif()

    set(Boost_INCLUDE_DIR "${NUGET_PATH}/boost.1.58.0.0/lib/native/include")
  endif()
else()
  message(FATAL_ERROR "-- Unsupported Build Platform.")
endif()

# Compiler (not platform) specific settings
if(ANDROID)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-strict-aliasing")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pedantic")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-attributes -Wno-pointer-arith")
  include_directories(SYSTEM
    "${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/4.8/include"
    "${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/4.8/libs/armeabi-v7a/include"
    "${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/4.8/include/backward"
    )
elseif((CMAKE_CXX_COMPILER_ID MATCHES "Clang") OR IOS)
  message("-- Setting clang options")

  set(WARNINGS "-Wall -Wextra -Wcast-qual -Wconversion -Wformat=2 -Winit-self -Winvalid-pch -Wmissing-format-attribute -Wmissing-include-dirs -Wpacked -Wredundant-decls")
  set(OSX_SUPPRESSIONS "-Wno-overloaded-virtual -Wno-sign-conversion -Wno-deprecated -Wno-unknown-pragmas -Wno-reorder -Wno-char-subscripts -Wno-switch -Wno-unused-parameter -Wno-unused-variable -Wno-deprecated -Wno-unused-value -Wno-unknown-warning-option -Wno-return-type-c-linkage -Wno-unused-function -Wno-sign-compare -Wno-shorten-64-to-32 -Wno-reorder -Wno-unused-local-typedefs")
  set(WARNINGS "${WARNINGS} ${OSX_SUPPRESSIONS}")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -Wno-return-type-c-linkage -Wno-unneeded-internal-declaration")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++11")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-strict-aliasing")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  message("-- Setting gcc options")

  set(WARNINGS "-Wall -Wextra -Wunused-parameter -Wcast-align -Wcast-qual -Wconversion -Wformat=2 -Winit-self -Winvalid-pch -Wmissing-format-attribute -Wmissing-include-dirs -Wpacked -Wredundant-decls -Wunreachable-code")

  set(LD_FLAGS "${LD_FLAGS} -Wl,-z,defs")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-strict-aliasing")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  message("-- Setting msvc options")
  set(WARNINGS)
  set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /ignore:4264")
  add_compile_options(/bigobj)
  if (WINDOWS_STORE OR WINDOWS_PHONE)
    add_compile_options(/ZW)
  endif()
else()
  message("-- Unknown compiler, success is doubtful.")
  message("CMAKE_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}")
endif()

if (CPPREST_EXCLUDE_WEBSOCKETS)
  add_definitions(-DCPPREST_EXCLUDE_WEBSOCKETS=1)
endif()

# Reconfigure final output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)

# These settings can be used by the test targets
set(Casablanca_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)
if (NOT CPPREST_EXCLUDE_WEBSOCKETS)
  find_path(WEBSOCKETPP_CONFIG websocketpp-config.cmake
                        HINTS /usr/lib/cmake/websocketpp)
  find_path(WEBSOCKETPP_CONFIG_VERSION websocketpp-configVersion.cmake
                        HINTS /usr/lib/cmake/websocketpp)
  if(WEBSOCKETPP_CONFIG AND WEBSOCKETPP_CONFIG_VERSION)
    include(${WEBSOCKETPP_CONFIG}/websocketpp-config.cmake)
    include(${WEBSOCKETPP_CONFIG}/websocketpp-configVersion.cmake)
    message("-- Found websocketpp version " ${PACKAGE_VERSION} " on system")
    set(Casablanca_SYSTEM_INCLUDE_DIRS ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${WEBSOCKETPP_INCLUDE_DIR})
  else(WEBSOCKETPP_CONFIG AND WEBSOCKETPP_CONFIG_VERSION)
    set(Casablanca_SYSTEM_INCLUDE_DIRS ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/libs/websocketpp)
    message("-- websocketpp not found, using the embedded version")
  endif(WEBSOCKETPP_CONFIG AND WEBSOCKETPP_CONFIG_VERSION)
endif()

set(Casablanca_LIBRARY cpprest)
set(Casablanca_LIBRARIES cpprest ${Boost_LIBRARIES})

# Everything in the project needs access to the casablanca include directories
include_directories(       ${Casablanca_INCLUDE_DIRS})
include_directories(SYSTEM ${Casablanca_SYSTEM_INCLUDE_DIRS})

# Finally, the tests all use the same style declaration to build themselves, so we use a function
function(add_casablanca_test NAME SOURCES_VAR)
  add_library(${NAME} ${TEST_LIBRARY_TARGET_TYPE} ${${SOURCES_VAR}})
  message("-- Added test library ${NAME}")
  if (NOT TEST_LIBRARY_TARGET_TYPE STREQUAL "OBJECT")
    target_link_libraries(${NAME}
      httptest_utilities
      common_utilities
      unittestpp
      ${Casablanca_LIBRARIES}
      ${ANDROID_STL_FLAGS}
      )
    if (BUILD_SHARED_LIBS)
      add_test(NAME ${NAME}
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMAND test_runner $<TARGET_FILE_NAME:${NAME}>
        )
    endif()
  endif()
endfunction()

add_subdirectory(src)

if(BUILD_TESTS)
  add_subdirectory(tests)
endif()

if(BUILD_SAMPLES)
  add_subdirectory(samples)
endif()
