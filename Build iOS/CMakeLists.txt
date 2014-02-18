project(casablanca-ios)
cmake_minimum_required(VERSION 2.6)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/build.simulator ${CMAKE_BINARY_DIR}/build.arm)

## Simulator version
add_custom_command(
  OUTPUT build.simulator/CMakeCache.txt
  COMMAND cmake ${CMAKE_SOURCE_DIR}/../Release/ -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/ios-cmake/toolchain/iOS.cmake -DIOS_PLATFORM=SIMULATOR -GXcode -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  WORKING_DIRECTORY build.simulator
)

add_custom_command(
  OUTPUT build.simulator/Binaries/Debug/libcasablanca.a
  COMMAND cmake --build . --target casablanca
  DEPENDS build.simulator/CMakeCache.txt
  WORKING_DIRECTORY build.simulator
)

## ARM version
add_custom_command(
  OUTPUT build.arm/CMakeCache.txt
  COMMAND cmake ${CMAKE_SOURCE_DIR}/../Release/ -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/ios-cmake/toolchain/iOS.cmake -DIOS_PLATFORM=OS -DCMAKE_OSX_ARCHITECTURES='armv7;armv7s;armv6' -GXcode -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  WORKING_DIRECTORY build.arm
)

add_custom_command(
  OUTPUT build.arm/Binaries/Debug/libcasablanca.a
  COMMAND cmake --build . --target casablanca
  DEPENDS build.arm/CMakeCache.txt
  WORKING_DIRECTORY build.arm
)

add_custom_command(
  OUTPUT libcasablanca.a
  COMMAND lipo -create -output ${CMAKE_CURRENT_BINARY_DIR}/libcasablanca.a
    ${CMAKE_CURRENT_BINARY_DIR}/build.arm/Binaries/Debug/libcasablanca.a
    ${CMAKE_CURRENT_BINARY_DIR}/build.simulator/Binaries/Debug/libcasablanca.a
  DEPENDS build.arm/Binaries/Debug/libcasablanca.a build.simulator/Binaries/Debug/libcasablanca.a
)

add_custom_target(casablanca ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/libcasablanca.a)
