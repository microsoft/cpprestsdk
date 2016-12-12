function(cpprest_find_zlib)
  if(ZLIB_FOUND)
    return()
  endif()

  if(APPLE AND NOT IOS)
    # Prefer the homebrew version of zlib
    find_library(ZLIB_LIBRARIES NAMES libz.a PATHS /usr/local/Cellar/zlib/1.2.8/lib NO_DEFAULT_PATH)
    find_path(ZLIB_INCLUDE_DIRS NAMES zlib.h PATHS /usr/local/Cellar/zlib/1.2.8/include NO_DEFAULT_PATH)

    if(NOT ZLIB_LIBRARIES OR NOT ZLIB_INCLUDE_DIRS)
      find_package(ZLIB REQUIRED)
    endif()
  else()
    find_package(ZLIB REQUIRED)
  endif()

  set(ZLIB_FOUND 1 CACHE INTERNAL "")
  set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIRS} CACHE INTERNAL "")
  set(ZLIB_LIBRARIES ${ZLIB_LIBRARIES} CACHE INTERNAL "")
endfunction()