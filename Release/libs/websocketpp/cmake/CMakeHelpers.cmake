
# Print build configuration
macro (print_used_build_config)
    message ("\n=========== Used Build Configuration =============\n")
    message (STATUS "ENABLE_CPP11        = " ${ENABLE_CPP11})
    message (STATUS "BUILD_EXAMPLES      = " ${BUILD_EXAMPLES})
    message (STATUS "BUILD_TESTS         = " ${BUILD_TESTS})
    message ("")
    message (STATUS "WEBSOCKETPP_ROOT    = " ${WEBSOCKETPP_ROOT})
    message (STATUS "WEBSOCKETPP_BIN     = " ${WEBSOCKETPP_BIN})
    message (STATUS "WEBSOCKETPP_LIB     = " ${WEBSOCKETPP_LIB})
    message (STATUS "Install prefix      = " ${CMAKE_INSTALL_PREFIX})
    message ("")
    message (STATUS "WEBSOCKETPP_BOOST_LIBS        = ${WEBSOCKETPP_BOOST_LIBS}")
    message (STATUS "WEBSOCKETPP_PLATFORM_LIBS     = ${WEBSOCKETPP_PLATFORM_LIBS}")
    message (STATUS "WEBSOCKETPP_PLATFORM_TSL_LIBS = ${WEBSOCKETPP_PLATFORM_TSL_LIBS}")
    message ("") 
endmacro ()

# Adds the given folder_name into the source files of the current project. 
# Use this macro when your module contains .cpp and .h files in several subdirectories.
# Your sources variable needs to be WSPP_SOURCE_FILES and headers variable WSPP_HEADER_FILES.
macro(add_source_folder folder_name)
    file(GLOB H_FILES_IN_FOLDER_${folder_name} ${folder_name}/*.hpp ${folder_name}/*.h)
    file(GLOB CPP_FILES_IN_FOLDER_${folder_name} ${folder_name}/*.cpp ${folder_name}/*.c)
    source_group("Header Files\\${folder_name}" FILES ${H_FILES_IN_FOLDER_${folder_name}})
    source_group("Source Files\\${folder_name}" FILES ${CPP_FILES_IN_FOLDER_${folder_name}})
    set(WSPP_HEADER_FILES ${WSPP_HEADER_FILES} ${H_FILES_IN_FOLDER_${folder_name}})
    set(WSPP_SOURCE_FILES ${WSPP_SOURCE_FILES} ${CPP_FILES_IN_FOLDER_${folder_name}})
endmacro()

# Initialize target.
macro (init_target NAME)
    set (TARGET_NAME ${NAME})
    message ("** " ${TARGET_NAME})

    # Include our own module path. This makes #include "x.h" 
    # work in project subfolders to include the main directory headers.
    include_directories (${CMAKE_CURRENT_SOURCE_DIR})
endmacro ()

# Build executable for executables
macro (build_executable TARGET_NAME)
    set (TARGET_LIB_TYPE "EXECUTABLE")
    message (STATUS "-- Build Type:")
    message (STATUS "       " ${TARGET_LIB_TYPE})

    add_executable (${TARGET_NAME} ${ARGN})

    include_directories (${WEBSOCKETPP_ROOT} ${WEBSOCKETPP_INCLUDE})

    set_target_properties (${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${WEBSOCKETPP_BIN})
    set_target_properties (${TARGET_NAME} PROPERTIES DEBUG_POSTFIX d)
endmacro ()

# Finalize target for all types
macro (final_target)
    if ("${TARGET_LIB_TYPE}" STREQUAL "EXECUTABLE")
        install (TARGETS ${TARGET_NAME} 
                 RUNTIME DESTINATION "bin" 
                 CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES})
    endif ()

    # install headers, directly from current source dir and look for subfolders with headers
    file (GLOB_RECURSE TARGET_INSTALL_HEADERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)
    foreach (hppfile ${TARGET_INSTALL_HEADERS})
      get_filename_component (currdir ${hppfile} PATH)
      install (FILES ${hppfile} DESTINATION "include/${TARGET_NAME}/${currdir}")
    endforeach()
endmacro ()

macro (link_boost)
    target_link_libraries (${TARGET_NAME} ${Boost_LIBRARIES})
endmacro ()

macro (link_openssl)
    target_link_libraries (${TARGET_NAME} ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
endmacro ()
