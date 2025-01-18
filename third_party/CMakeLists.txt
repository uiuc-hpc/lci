if(LCIXX_COMPILE_REG_CACHE)
  add_subdirectory(ucx)
  target_include_directories(LCIXX PRIVATE ucx)
  target_link_libraries(LCIXX PRIVATE lci-ucx)
endif()

add_subdirectory(ConcurrencyFreaks)
add_subdirectory(lprq)
