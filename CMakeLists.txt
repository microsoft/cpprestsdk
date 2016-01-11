set(CMAKE_LEGACY_CYGWIN_WIN32 0)
cmake_minimum_required(VERSION 3.0)

include("Release/cmake/HunterGate.cmake")
HunterGate(
    URL "http://192.168.2.27/packages/hunter/hunter-project-v1.0.16.tar.gz"
    SHA1 "ef89bcd473ceef3aa9a3ae085dfc68b92b927639"
)

project(casablanca)
add_subdirectory(Release)