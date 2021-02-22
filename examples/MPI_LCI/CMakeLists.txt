find_package(MPI)

function(add_mplci_benchmark NAME)
    set(TARGET_NAME mplci_${NAME})
    add_executable(${TARGET_NAME} ${NAME}.cpp)
    target_link_libraries(${TARGET_NAME} PRIVATE lci_shared)
    target_include_directories(${TARGET_NAME} PRIVATE ${MPI_CXX_INCLUDE_PATH})
    target_compile_options(${TARGET_NAME} PRIVATE ${MPI_CXX_COMPILE_FLAGS})
    target_link_libraries(${TARGET_NAME} PRIVATE ${MPI_CXX_LIBRARIES} ${MPI_CXX_LINK_FLAGS})
    target_compile_definitions(${TARGET_NAME} PRIVATE YAMPI_MPI)
endfunction()

function(add_mplci_benchmarks)
    foreach(name ${ARGN})
        add_mplci_benchmark(${name})
    endforeach()
endfunction()

if(MPI_FOUND)
    add_mplci_benchmarks(
            hello_world
    )
endif()