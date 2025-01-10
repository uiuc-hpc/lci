function(add_lci_examples)
  foreach(name ${ARGN})
    string(REGEX REPLACE "\\.[^.]*$" "" name_without_ext ${name})
    add_lci_executable(${name_without_ext} ${name} comm_exp.h)
  endforeach()
  add_lci_tests(TESTS ${ARGN} LABELS example COMMANDS
                "${LCIXX_USE_CTEST_EXE} -n 2 ${LCIXX_USE_CTEST_ARGS} [TARGET]")
endfunction()

option(LCI_BUILD_EXAMPLES "Build examples by default" ON)
if(NOT LCI_BUILD_EXAMPLES)
  set(EXCLUDE_FROM_ALL ON)
endif()
add_lci_examples(
  hello_world.c
  pt2pts_sync.c
  pt2ptm_queue.c
  pt2ptl_handler.c
  puts_handler.c
  putmna_sync.c
  putla_queue.c
  many2many_random.cpp)
