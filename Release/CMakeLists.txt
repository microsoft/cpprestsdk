set(CMAKE_LEGACY_CYGWIN_WIN32 0)
cmake_minimum_required(VERSION 2.6)
project(casablanca)

enable_testing()

set(WARNINGS "-Wall -Wextra -Wunused-parameter -Wcast-align -Wcast-qual -Wconversion -Wformat=2 -Winit-self -Winvalid-pch -Wmissing-format-attribute -Wmissing-include-dirs -Wpacked -Wredundant-decls -Wunreachable-code")

set(LINUX_SUPPRESSIONS "-Wno-deprecated -Wno-unknown-pragmas -Wno-reorder -Wno-unused-function -Wno-char-subscripts -Wno-switch -Wno-unused-but-set-parameter -Wno-deprecated -Wno-unused-value -Wno-unused-local-typedefs")

set(OSX_SUPPRESSIONS "-Wno-overloaded-virtual -Wno-sign-conversion -Wno-deprecated -Wno-unknown-pragmas -Wno-reorder -Wno-unused-function -Wno-char-subscripts -Wno-switch -Wno-unused-parameter -Wno-unused-variable -Wno-deprecated -Wno-unused-value -Wno-unknown-warning-option -Wno-return-type-c-linkage -Wno-unused-function -Wno-unused-variable -Wno-unneeded-internal-declaration -Wno-return-type-c-linkage")

if(IOS)
  set(Boost_FRAMEWORK "-F ${CMAKE_SOURCE_DIR}/.. -framework boost")
  set(Boost_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../boost.framework/Headers")

  set(OPENSSL_FOUND 1)
  set(OPENSSL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../openssl-include")
  set(OPENSSL_LIBRARIES
    "${CMAKE_SOURCE_DIR}/../openssl-ios/lib/libcrypto.a"
    "${CMAKE_SOURCE_DIR}/../openssl-ios/lib/libssl.a"
    )

  set(BUILD_SHARED_LIBS OFF)

  set(LIB)
elseif(UNIX)
  find_package(Boost COMPONENTS system thread locale regex filesystem REQUIRED)
  find_package(Threads)
  find_package(OpenSSL REQUIRED)

  option(BUILD_SHARED_LIBS "Build shared Libraries." ON)

  set(LIB)
else()
  error("Unsupported Build Platform.")
endif()

if(("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang") OR IOS)
  message("-- Setting clang options")

  set(WARNINGS "${WARNINGS} ${OSX_SUPPRESSIONS}")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -Wno-return-type-c-linkage")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++11")
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  message("-- Setting gcc options")

  set(WARNINGS "${WARNINGS} ${LINUX_SUPPRESSIONS}")
  set(LD_FLAGS "${LD_FLAGS} -Wl,-z,defs")
else()
  message("-- Unknown compiler, success is doubtful.")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-strict-aliasing")

set(STRICT_CXX_FLAGS ${WARNINGS} "-Werror -pedantic")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)

set(Casablanca_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(Casablanca_LIBRARIES ${LIB}casablanca)

include_directories(${Casablanca_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIR})

add_subdirectory(src)
add_subdirectory(tests)
if(NOT IOS)
  add_subdirectory(samples EXCLUDE_FROM_ALL)
endif()
