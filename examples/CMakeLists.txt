function(add_lci_examples)
  foreach(name ${ARGN})
    string(REGEX REPLACE "\\.[^.]*$" "" name_without_ext ${name})
    add_lci_executable(${name_without_ext} ${name})
  endforeach()
  add_lci_tests(TESTS ${ARGN} LABELS example COMMANDS
                "${LCI_USE_CTEST_LAUNCHER} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET]")
endfunction()

option(LCI_BUILD_EXAMPLES "Build examples by default" ON)
if(NOT LCI_BUILD_EXAMPLES)
  set(EXCLUDE_FROM_ALL ON)
endif()
add_lci_examples(hello_world.cpp hello_world_am.cpp pingpong_am_mt.cpp
                 nonblocking_barrier.cpp distributed_array.cpp)
