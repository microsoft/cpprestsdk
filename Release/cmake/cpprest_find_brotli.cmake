function(cpprest_find_brotli)
  if(TARGET cpprestsdk_brotli_internal)
    return()
  endif()

  find_package(UNOFFICIAL-BROTLI REQUIRED)

  add_library(cpprestsdk_brotli_internal INTERFACE)
  target_link_libraries(cpprestsdk_brotli_internal INTERFACE unofficial::brotli::brotlienc unofficial::brotli::brotlidec unofficial::brotli::brotlicommon)
endfunction()
