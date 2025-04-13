function(add_lci_benchmarks)
  foreach(name ${ARGN})
    string(REGEX REPLACE "\\.[^.]*$" "" name_without_ext ${name})
    add_lci_executable(${name_without_ext} ${name})
  endforeach()
  add_lci_tests(TESTS ${ARGN} LABELS benchmark COMMANDS
                "${LCI_USE_CTEST_LAUNCHER} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET]")
endfunction()

option(LCI_BUILD_BENCHMARKS "Build benchmarks by default" ON)
if(NOT LCI_BUILD_BENCHMARKS)
  set(EXCLUDE_FROM_ALL ON)
endif()
add_lci_benchmarks(collective.cpp)

add_subdirectory(resources)
