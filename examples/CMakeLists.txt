function(add_example NAME FILE)
  add_executable(${NAME} ${FILE})
  target_compile_definitions(${NAME} PRIVATE _GNU_SOURCE)
  target_link_libraries(${NAME} PRIVATE
    Threads::Threads
    lci_static
  )
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.13")
    target_link_options(${NAME} PRIVATE
      LINKER:-z,now
      LINKER:-z,relro
    )
  else()
    set_property(TARGET ${NAME} APPEND_STRING PROPERTY LINK_FLAGS
      " -Wl,-z,now -Wl,-z,relro"
    )
  endif()
  set_target_properties(${NAME} PROPERTIES
    C_STANDARD                99
    C_EXTENSIONS              ON
    CUDA_STANDARD             11
  )
  install(TARGETS ${NAME} DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/lc)
endfunction()

function(add_all_examples)
  foreach(example ${ARGN})
    add_example(${example} ${example}.c)
  endforeach()
endfunction()

add_all_examples(
  init
  pt2pti
  pt2ptbc
  pt2ptd
  pt2ptiq
  puti
  putb
  putbc
)

if(LCI_CUDA)
  add_example(pt2ptbc_cuda pt2ptbc_cuda.cu)
endif()
