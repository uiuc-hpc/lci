set(LCI_PM_BACKEND_DEFAULT
    "pmix;pmi2;pmi1;mpi;local"
    CACHE
      STRING
      "A list of process management backend with comma/space/semicolon as delimitors.
      LCI will try each entry one by one until it found an available backend to use
      (available entries: pmi1, pmi2, pmix, mpi, local)")

set(LIBRARY_NAME LCI)
target_sources_relative(
  ${LIBRARY_NAME} PRIVATE pmi_wrapper.c pmi_wrapper_pmi1.c pmi_wrapper_pmi2.c
  pmi_wrapper_local.c)
add_subdirectory(pmi1)
add_subdirectory(pmi2)

find_package(MPI COMPONENTS C)
if(MPI_FOUND)
  set(LCI_PM_BACKEND_ENABLE_MPI
      ON
      CACHE STRING "Enable MPI as the process management backend")
  target_sources_relative(${LIBRARY_NAME} PRIVATE pmi_wrapper_mpi.c)
  cmake_policy(SET CMP0079 NEW)
  target_link_libraries(${LIBRARY_NAME} PRIVATE MPI::MPI_C)
endif()

find_package(PMIx COMPONENTS C)
if(PMIX_FOUND)
  set(LCI_PM_BACKEND_ENABLE_PMIX
      ON
      CACHE STRING "Enable PMIx as the process management backend")
  target_sources_relative(${LIBRARY_NAME} PRIVATE pmi_wrapper_pmix.c)
  cmake_policy(SET CMP0079 NEW)
  target_link_libraries(${LIBRARY_NAME} PRIVATE PMIx::PMIx)
endif()

target_include_directories(${LIBRARY_NAME} PRIVATE .)
