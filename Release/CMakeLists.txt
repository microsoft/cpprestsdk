set(CMAKE_LEGACY_CYGWIN_WIN32 0)
cmake_minimum_required(VERSION 3.0)

include("cmake/HunterGate.cmake")
HunterGate(
    URL "http://192.168.2.27/packages/hunter/hunter-project-v1.0.16.tar.gz"
    SHA1 "ef89bcd473ceef3aa9a3ae085dfc68b92b927639"
)

project(casablanca)

enable_testing()

set(WARNINGS)
set(ANDROID_STL_FLAGS)

# Platform (not compiler) specific settings
if(IOS)
  set(IOS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Build_iOS")
  add_definitions( -DBOOST_ALL_NO_LIB )
  hunter_add_package(Boost COMPONENTS random system thread filesystem chrono atomic)

  hunter_add_package(OpenSSL)
  find_package(OpenSSL REQUIRED)

  # The cxx_flags must be reset here, because the ios-cmake toolchain file unfortunately sets "-headerpad_max_install_names" which is not a valid clang flag.
  set(CMAKE_CXX_FLAGS "-fvisibility=hidden -fvisibility-inlines-hidden")

  set(BUILD_SHARED_LIBS OFF)
  set(BUILD_SAMPLES OFF)
  option(BUILD_TESTS "Build tests." OFF)
elseif(ANDROID)
  add_definitions( -DBOOST_ALL_NO_LIB )
  hunter_add_package(Boost COMPONENTS random system thread filesystem chrono atomic)

  hunter_add_package(OpenSSL)
  find_package(OpenSSL REQUIRED)

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
  option(BUILD_TESTS "Build tests." OFF)
elseif(UNIX) # This includes OSX
    add_definitions( -DBOOST_ALL_NO_LIB )
  hunter_add_package(Boost COMPONENTS random system thread filesystem chrono atomic)
  find_package(Threads REQUIRED)
  
  hunter_add_package(OpenSSL)
  find_package(OpenSSL REQUIRED)

  option(BUILD_SHARED_LIBS "Build shared Libraries." OFF)
  option(BUILD_TESTS "Build tests." OFF)
  option(BUILD_SAMPLES "Build samples." OFF)
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
  option(BUILD_SHARED_LIBS "Build shared Libraries." OFF)
  option(BUILD_TESTS "Build tests." OFF)
  option(BUILD_SAMPLES "Build samples." OFF)
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

  add_definitions( -DBOOST_ALL_NO_LIB )
  hunter_add_package(Boost COMPONENTS random system thread filesystem chrono atomic)
  find_package(Boost CONFIG REQUIRED random system thread filesystem chrono atomic)

  hunter_add_package(OpenSSL)
  find_package(OpenSSL REQUIRED)
else()
  message(FATAL_ERROR "-- Unsupported Build Platform.")
endif()

# Compiler (not platform) specific settings
if(ANDROID)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-strict-aliasing")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pedantic")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-attributes -Wno-pointer-arith")
  include_directories(
    "${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/4.8/include"
    "${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/4.8/libs/armeabi-v7a/include"
    "${ANDROID_NDK}/sources/cxx-stl/gnu-libstdc++/4.8/include/backward"
    )
elseif((CMAKE_CXX_COMPILER_ID MATCHES "Clang") OR IOS)
  message("-- Setting clang options")

  set(WARNINGS "-Wall -Wextra -Wcast-qual -Wconversion -Wformat=2 -Winit-self -Winvalid-pch -Wmissing-format-attribute -Wmissing-include-dirs -Wpacked -Wredundant-decls")
  set(OSX_SUPPRESSIONS "-Wno-overloaded-virtual -Wno-sign-conversion -Wno-deprecated -Wno-unknown-pragmas -Wno-reorder -Wno-char-subscripts -Wno-switch -Wno-unused-parameter -Wno-unused-variable -Wno-deprecated -Wno-unused-value -Wno-unknown-warning-option -Wno-return-type-c-linkage -Wno-unused-function -Wno-sign-compare -Wno-shorten-64-to-32 -Wno-reorder")
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
  ADD_DEFINITIONS(-D_NO_ASYNCRTIMP) 
  ADD_DEFINITIONS(-D_NO_PPLXIMP)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
  set(WARNINGS)
else()
  message("-- Unknown compiler, success is doubtful.")
  message("CMAKE_CXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}")
endif()

# Reconfigure final output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)

# These settings can be used by the test targets
set(Casablanca_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_path(WEBSOCKETPP_CONFIG websocketpp-config.cmake
                       HINTS /usr/lib/cmake/websocketpp)
find_path(WEBSOCKETPP_CONFIG_VERSION websocketpp-configVersion.cmake
                       HINTS /usr/lib/cmake/websocketpp)
if(WEBSOCKETPP_CONFIG AND WEBSOCKETPP_CONFIG_VERSION)
  include(${WEBSOCKETPP_CONFIG}/websocketpp-config.cmake)
  include(${WEBSOCKETPP_CONFIG}/websocketpp-configVersion.cmake)
  message("-- Found websocketpp version " ${PACKAGE_VERSION} " on system")
  set(Casablanca_INCLUDE_DIRS ${Casablanca_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${WEBSOCKETPP_INCLUDE_DIR})
else(WEBSOCKETPP_CONFIG AND WEBSOCKETPP_CONFIG_VERSION)
  set(Casablanca_INCLUDE_DIRS ${Casablanca_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/libs/websocketpp)
  message("-- websocketpp not found, using the embedded version")
endif(WEBSOCKETPP_CONFIG AND WEBSOCKETPP_CONFIG_VERSION)

set(Casablanca_LIBRARY ${LIB}cpprest)
set(Casablanca_LIBRARIES ${Casablanca_LIBRARY} ${Boost_LIBRARIES} ${Boost_FRAMEWORK})

# Everything in the project needs access to the casablanca include directories
include_directories(${Casablanca_INCLUDE_DIRS})

# Finally, the tests all use the same style declaration to build themselves, so we use a function
function(add_casablanca_test NAME SOURCES_VAR)
  add_library(${NAME} ${TEST_LIBRARY_TARGET_TYPE} ${${SOURCES_VAR}})
  message("-- Added test library ${NAME}")
  if (NOT TEST_LIBRARY_TARGET_TYPE STREQUAL "OBJECT")
    target_link_libraries(${NAME}
      ${LIB}httptest_utilities
      ${LIB}common_utilities
      ${LIB}unittestpp
      ${Casablanca_LIBRARIES}
      ${ANDROID_STL_FLAGS}
      )
    if (BUILD_SHARED_LIBS)
      add_test(NAME ${NAME}
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMAND test_runner lib${NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
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
