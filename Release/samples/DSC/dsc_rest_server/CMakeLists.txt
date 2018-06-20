SET (Boost_INCLUDE_DIRS "C:/src/vcpkg/installed/x64-windows/include")
SET (Boost_LIBRARIES "C:/src/vcpkg/installed/x64-windows/debug/lib")

include_directories(${Boost_INCLUDE_DIRS}) 

add_executable(dsc_rest_server
  dsc_rest_server.cpp
)

target_link_libraries(dsc_rest_server
    ${Boost_LIBRARIES}/boost_system-vc140-mt-gd.lib
    cpprest)
