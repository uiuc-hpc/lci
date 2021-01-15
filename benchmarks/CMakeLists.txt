function(add_benchmark NAME)
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

function(add_benchmarks)
  foreach(name ${ARGN})
    add_benchmark(${name})
  endforeach()
endfunction()

add_benchmarks(
        pingpong
        pingpong2
        pingpong_mt
        pingpong_mt2
)