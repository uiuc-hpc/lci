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
option(LCI_PM_BACKEND_ENABLE_MPI "Enable MPI as the process management backend"
       ${MPI_FOUND})
if(LCI_PM_BACKEND_ENABLE_MPI)
  if(MPI_FOUND)
    target_sources_relative(${LIBRARY_NAME} PRIVATE pmi_wrapper_mpi.c)
    cmake_policy(SET CMP0079 NEW)
    target_link_libraries(${LIBRARY_NAME} PRIVATE MPI::MPI_C)
  else()
    message(
      FATAL_ERROR
        "Users explicitly ask to enable MPI as the process management backend, but MPI is not found."
    )
  endif()
endif()

find_package(PMIx)
option(LCI_PM_BACKEND_ENABLE_PMIX
       "Enable PMIx as the process management backend" ${PMIX_FOUND})
if(LCI_PM_BACKEND_ENABLE_PMIX)
  if(PMIX_FOUND)
    target_sources_relative(${LIBRARY_NAME} PRIVATE pmi_wrapper_pmix.c)
    cmake_policy(SET CMP0079 NEW)
    target_link_libraries(${LIBRARY_NAME} PRIVATE PMIx::PMIx)
  else()
    message(
      FATAL_ERROR
        "Users explicitly ask to enable PMIx as the process management backend, but MPI is not found."
    )
  endif()
endif()

target_include_directories(${LIBRARY_NAME} PRIVATE .)
