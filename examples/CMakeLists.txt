function(add_lci_examples)
  foreach(name ${ARGN})
    string(REGEX REPLACE "\\.[^.]*$" "" name_without_ext ${name})
    add_lci_executable(${name_without_ext} ${name} comm_exp.h)
  endforeach()
endfunction()

option(LCI_BUILD_EXAMPLES "Build examples by default" ON)
if(NOT LCI_BUILD_EXAMPLES)
  set(EXCLUDE_FROM_ALL ON)
endif()
add_lci_examples(hello_world.c pt2pts_sync.c pt2ptm_queue.c pt2ptl_handler.c)

# add_subdirectory(MPI_LCI)
