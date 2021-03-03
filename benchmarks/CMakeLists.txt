find_package(OpenMP REQUIRED)
find_package(PAPI)
option(USE_PAPI "Use PAPI to collect hardware counters" ${PAPI_FOUND})
configure_file(bench_config.h.in bench_config.h @ONLY)

function(add_lci_benchmark name)
  string(REGEX REPLACE "\\.[^.]*$" "" name_without_ext ${name})
  add_lci_executable(${name_without_ext} ${name} comm_exp.h)
  target_include_directories(${name_without_ext} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  target_link_libraries(${name_without_ext} PRIVATE OpenMP::OpenMP_CXX)
  if(USE_PAPI)
    target_link_libraries(${name_without_ext} PRIVATE Papi::papi)
  endif()
endfunction()

function(add_lci_benchmarks)
  foreach(name ${ARGN})
    add_lci_benchmark(${name})
  endforeach()
endfunction()

add_lci_benchmarks(
        pingpong.cpp
        pingpong2.cpp
        pingpong_mt.cpp
        pingpong_mt2.cpp
)