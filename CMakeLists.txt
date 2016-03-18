set(CMAKE_LEGACY_CYGWIN_WIN32 0)
cmake_minimum_required(VERSION 3.0)

include("Release/cmake/HunterGate.cmake")
HunterGate(
    URL "http://192.168.2.27/packages/hunter/hunter-project-v1.0.60.tar.gz"
    SHA1 "e1a3b70805d481652b53737e4920b4dd712ad067"
)

project(casablanca)
add_subdirectory(Release)