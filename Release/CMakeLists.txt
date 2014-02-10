set(CMAKE_LEGACY_CYGWIN_WIN32 0)
cmake_minimum_required(VERSION 2.6)
project(casablanca)

enable_testing()

set(WARNINGS)

# Platform (not compiler) specific settings
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
elseif(UNIX)
  find_package(Boost COMPONENTS system thread locale regex filesystem REQUIRED)
  find_package(Threads)
  find_package(OpenSSL REQUIRED)

  option(BUILD_SHARED_LIBS "Build shared Libraries." ON)
elseif(WIN32)
  option(BUILD_SHARED_LIBS "Build shared Libraries." ON)

  add_definitions(-DUNICODE)

  if(NOT BUILD_SHARED_LIBS)
    # This causes cmake to not link the test libraries separately, but instead hold onto their object files.
    set(TEST_LIBRARY_TARGET_TYPE OBJECT)
  endif()

  set(LIB lib)
else()
  message("-- Unsupported Build Platform.")
endif()

# Compiler (not platform) specific settings
if(("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang") OR IOS)
  message("-- Setting clang options")

  set(WARNINGS "-Wall -Wextra -Wunused-parameter -Wcast-align -Wcast-qual -Wconversion -Wformat=2 -Winit-self -Winvalid-pch -Wmissing-format-attribute -Wmissing-include-dirs -Wpacked -Wredundant-decls -Wunreachable-code")
  set(OSX_SUPPRESSIONS "-Wno-overloaded-virtual -Wno-sign-conversion -Wno-deprecated -Wno-unknown-pragmas -Wno-reorder -Wno-unused-function -Wno-char-subscripts -Wno-switch -Wno-unused-parameter -Wno-unused-variable -Wno-deprecated -Wno-unused-value -Wno-unknown-warning-option -Wno-return-type-c-linkage -Wno-unused-function -Wno-unused-variable -Wno-unneeded-internal-declaration -Wno-return-type-c-linkage")
  set(WARNINGS "${WARNINGS} ${OSX_SUPPRESSIONS}")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -Wno-return-type-c-linkage")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++11")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-strict-aliasing")
  set(STRICT_CXX_FLAGS ${WARNINGS} "-Werror -pedantic")
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  message("-- Setting gcc options")

  set(WARNINGS "-Wall -Wextra -Wunused-parameter -Wcast-align -Wcast-qual -Wconversion -Wformat=2 -Winit-self -Winvalid-pch -Wmissing-format-attribute -Wmissing-include-dirs -Wpacked -Wredundant-decls -Wunreachable-code")
  set(LINUX_SUPPRESSIONS "-Wno-deprecated -Wno-unknown-pragmas -Wno-reorder -Wno-unused-function -Wno-char-subscripts -Wno-switch -Wno-unused-but-set-parameter -Wno-deprecated -Wno-unused-value -Wno-unused-local-typedefs")

  set(WARNINGS "${WARNINGS} ${LINUX_SUPPRESSIONS}")
  set(LD_FLAGS "${LD_FLAGS} -Wl,-z,defs")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-strict-aliasing")
  set(STRICT_CXX_FLAGS ${WARNINGS} "-Werror -pedantic")
else()
  message("-- Unknown compiler, success is doubtful.")
endif()

# Reconfigure final output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/Binaries)

# These settings can be used by the test targets
set(Casablanca_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(Casablanca_LIBRARIES ${LIB}casablanca)

# Everything in the project needs access to the casablanca include directory
include_directories(${Casablanca_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIR})

# Finally, the tests all use the same style declaration to build themselves, so we use a function
function(add_casablanca_test NAME SOURCES_VAR)
  add_library(${NAME} ${TEST_LIBRARY_TARGET_TYPE} ${${SOURCES_VAR}})
  message("-- Added test library ${NAME}")
  if (NOT TEST_LIBRARY_TARGET_TYPE STREQUAL "OBJECT")
    target_link_libraries(${NAME}
      ${LIB}unittestpp
      ${LIB}httptest_utilities
      ${LIB}common_utilities
      ${Boost_LIBRARIES}
      ${Boost_FRAMEWORK}
      ${Casablanca_LIBRARIES}
      )

    add_test(NAME ${NAME}
      WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
      COMMAND test_runner lib${NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
      )
  endif()
endfunction()

add_subdirectory(src)
add_subdirectory(tests)
# If we aren't on iOS, we can build samples.
# These are also excluded from the "all" or default target
if(NOT IOS)
  add_subdirectory(samples EXCLUDE_FROM_ALL)
endif()
