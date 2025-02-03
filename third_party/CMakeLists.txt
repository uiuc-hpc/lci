if(LCI_COMPILE_REG_CACHE)
  add_subdirectory(ucx)
  target_include_directories(LCI PRIVATE ucx)
  target_link_libraries(LCI PRIVATE lci-ucx)
endif()

add_subdirectory(ConcurrencyFreaks)
add_subdirectory(lprq)
