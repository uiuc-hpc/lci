option(LCI_BUILD_MPLCI "build MPI/LCI interoperation examples by default" ON)
if(NOT LCI_BUILD_MPLCI)
  set(EXCLUDE_FROM_ALL ON)
endif()

find_package(MPI)

function(add_mplci_example NAME)
  set(TARGET_NAME mplci_${NAME})
  add_executable(${TARGET_NAME} ${NAME}.cpp)
  target_link_libraries(${TARGET_NAME} PRIVATE LCI)
  target_include_directories(${TARGET_NAME} PRIVATE ${MPI_CXX_INCLUDE_PATH})
  target_compile_options(${TARGET_NAME} PRIVATE ${MPI_CXX_COMPILE_FLAGS})
  target_link_libraries(${TARGET_NAME} PRIVATE ${MPI_CXX_LIBRARIES}
                                               ${MPI_CXX_LINK_FLAGS})
  target_compile_definitions(${TARGET_NAME} PRIVATE YAMPI_MPI)
endfunction()

function(add_mplci_examples)
  foreach(name ${ARGN})
    add_mplci_example(${name})
  endforeach()
endfunction()

if(MPI_FOUND)
  add_mplci_examples(hello_world)
endif()
