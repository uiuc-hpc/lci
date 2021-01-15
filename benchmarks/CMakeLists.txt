find_package(OpenMP REQUIRED)
find_package(PAPI)
option(USE_PAPI "Use PAPI to collect hardware counters" ${PAPI_FOUND})
configure_file(bench_config.h.in bench_config.h @ONLY)

function(add_benchmark NAME)
  add_executable(${NAME} ${NAME}.cpp)
  target_include_directories(${NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  target_compile_definitions(${NAME} PRIVATE _GNU_SOURCE)
  target_link_libraries(${NAME} PRIVATE
          Threads::Threads
          lci_static
          OpenMP::OpenMP_CXX
          )
  if(USE_PAPI)
    target_link_libraries(${NAME} PRIVATE Papi::papi)
  endif()
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