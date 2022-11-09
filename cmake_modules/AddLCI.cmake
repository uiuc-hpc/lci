function(add_lci_executable name)
  add_executable(${name} ${ARGN})
  target_compile_definitions(${name} PRIVATE _GNU_SOURCE)
  target_link_libraries(${name} PRIVATE LCI)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.13")
    target_link_options(${name} PRIVATE LINKER:-z,now LINKER:-z,relro)
  else()
    set_property(
      TARGET ${name}
      APPEND_STRING
      PROPERTY LINK_FLAGS " -Wl,-z,now -Wl,-z,relro")
  endif()
  set_target_properties(
    ${name}
    PROPERTIES C_STANDARD 99
               C_EXTENSIONS ON
               CXX_STANDARD 11)
endfunction()
