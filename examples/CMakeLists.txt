function(add_c_example NAME)
  add_executable(${NAME} ${NAME}.c)
  target_compile_definitions(${NAME} PRIVATE _GNU_SOURCE)
  target_link_libraries(${NAME} PRIVATE
    Threads::Threads
    lci_shared
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
  )
  install(TARGETS ${NAME} DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/lc)
endfunction()

function(add_all_c_examples)
  foreach(example ${ARGN})
    add_c_example(${example})
  endforeach()
endfunction()

function(add_cxx_example NAME)
  find_package(OpenMP REQUIRED)
  add_executable(${NAME} ${NAME}.cpp)
  target_compile_definitions(${NAME} PRIVATE _GNU_SOURCE)
  target_link_libraries(${NAME} PRIVATE
          Threads::Threads
          lci_static
          OpenMP::OpenMP_CXX
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
          CXX_STANDARD                11
          )
  install(TARGETS ${NAME} DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/lc)
endfunction()

function(add_all_cxx_examples)
  foreach(example ${ARGN})
    add_cxx_example(${example})
  endforeach()
endfunction()

function(add_tests)
  foreach(test ${ARGN})
    add_test(NAME test_${test}
             COMMAND ${TEST_EXE} ${TEST_ARG} -n 2 $<TARGET_FILE:${test}>)
  endforeach()
endfunction()

add_all_c_examples(
        init
        pt2pti
        pt2ptbc
        pt2ptd
        pt2ptiq
        puti
        putb
        putbc
)

add_all_cxx_examples(
        pt2ptbc_mt
        pt2ptbc_mt2
)

add_tests(
        init
        pt2pti
        pt2ptiq
        pt2ptbc
        pt2ptd
        puti
        putb
        putbc
        pt2ptbc_mt
        pt2ptbc_mt2
)