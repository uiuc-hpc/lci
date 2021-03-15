find_package(OpenMP REQUIRED)

function(add_lci_test name)
    if( NOT LCI_BUILD_TESTS )
        set(EXCLUDE_FROM_ALL ON)
    endif()

    cmake_parse_arguments(ARG "" "" "COMMAND;LABELS;SOURCES" ${ARGN})

    add_lci_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE OpenMP::OpenMP_CXX)
    string(REGEX REPLACE "\\[TARGET\\]" $<TARGET_FILE:${name}> TEST_COMMAND ${ARG_COMMAND})
    string(REPLACE " " ";" TEST_COMMAND ${TEST_COMMAND})
    add_test(NAME ${name}
             COMMAND ${TEST_COMMAND})
    set_property(TEST ${name} PROPERTY LABELS ${ARG_LABELS})
endfunction()

function(add_lci_tests)
    cmake_parse_arguments(ARG "" "" "COMMAND;LABELS;TESTS" ${ARGN})
    foreach(name ${ARG_TESTS})
        string(REGEX REPLACE "\\.[^.]*$" "" name_without_ext ${name})
        add_lci_test(test-${ARG_LABELS}-${name_without_ext} SOURCES ${name} LABELS ${ARG_LABELS} COMMAND ${ARG_COMMAND})
    endforeach()
endfunction()

option(LCI_BUILD_TESTS "Build tests by default" ON)
set(SRUN_EXE mpirun CACHE STRING "exective to be used in ctest")
set(SRUN_ARG "-n 2" CACHE STRING "arguments to be used in ctest")

include_directories(./)
add_subdirectory(gtests)
add_subdirectory(loopback)
add_subdirectory(pingpong)
