set(UnitTestpp_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Common/UnitTestpp)
set(Utilities_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Common/utilities/include)

include_directories (${UnitTestpp_INCLUDE_DIR} ${Utilities_INCLUDE_DIR})

add_subdirectory(Common)
add_subdirectory(Functional)
