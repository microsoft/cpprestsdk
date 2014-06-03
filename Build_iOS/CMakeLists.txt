project(casablanca-ios)
cmake_minimum_required(VERSION 2.6)

set(TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/ios-cmake/toolchain/iOS.cmake")

set(SIM_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/build.i386" CACHE INTERNAL "")
set(SIM_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Release" CACHE INTERNAL "")

set(ARM_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/build.arm" CACHE INTERNAL "")
set(ARM_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Release" CACHE INTERNAL "")

file(MAKE_DIRECTORY ${SIM_BINARY_DIR})
execute_process(WORKING_DIRECTORY ${SIM_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND}
    -GXcode
    -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}
    -DIOS_PLATFORM=SIMULATOR
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    "${SIM_SOURCE_DIR}"
    )

file(MAKE_DIRECTORY ${ARM_BINARY_DIR})
execute_process(WORKING_DIRECTORY ${ARM_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND}
    -GXcode
    -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}
    -DIOS_PLATFORM=OS
    "-DCMAKE_OSX_ARCHITECTURES=armv7s;armv7;arm64"
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    "${ARM_SOURCE_DIR}"
    )


## Simulator version
add_custom_target(sim
  COMMAND ${CMAKE_COMMAND}
    --build ${SIM_BINARY_DIR}
    --target cpprest
    --config ${CMAKE_BUILD_TYPE}
  COMMENT "Building for i386 (simulator)"
  VERBATIM
)

## ARM version
add_custom_target(arm
  COMMAND ${CMAKE_COMMAND}
    --build ${ARM_BINARY_DIR}
    --target cpprest
    --config ${CMAKE_BUILD_TYPE}
  COMMENT "Building for armv7, armv7s, arm64"
  VERBATIM
)

set(LIB_CPPREST libcpprest.a)
add_custom_command(
  OUTPUT ${LIB_CPPREST}
  COMMAND lipo -create
    -output "${CMAKE_CURRENT_BINARY_DIR}/${LIB_CPPREST}"
    ${SIM_BINARY_DIR}/Binaries/${CMAKE_BUILD_TYPE}/${LIB_CPPREST}
    ${ARM_BINARY_DIR}/Binaries/${CMAKE_BUILD_TYPE}/${LIB_CPPREST}
  DEPENDS
    sim
    arm
    "${SIM_BINARY_DIR}/Binaries/${CMAKE_BUILD_TYPE}/${LIB_CPPREST}"
    "${ARM_BINARY_DIR}/Binaries/${CMAKE_BUILD_TYPE}/${LIB_CPPREST}"
  VERBATIM
)

add_custom_target(cpprest ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${LIB_CPPREST})
