function(add_lci_test name)
    if( NOT LCI_BUILD_TESTS )
        set(EXCLUDE_FROM_ALL ON)
    endif()

    cmake_parse_arguments(ARG "" "" "LABELS;SOURCES" ${ARGN})

    add_lci_executable(${name} ${ARG_SOURCES})
    add_test(NAME ${name}
             COMMAND ${TEST_EXE} ${TEST_ARG} -n 2 $<TARGET_FILE:${name}>)
    set_property(TEST ${name} PROPERTY LABELS ${ARG_LABELS})
endfunction()

function(add_lci_tests)
    cmake_parse_arguments(ARG "" "" "LABELS;TESTS" ${ARGN})
    foreach(name ${ARG_TESTS})
        string(REGEX REPLACE "\\.[^.]*$" "" name_without_ext ${name})
        add_lci_test(test-${name_without_ext} SOURCES ${name} LABELS ${ARG_LABELS})
    endforeach()
endfunction()

option(LCI_BUILD_TESTS "Build tests by default" ON)
set(TEST_EXE mpirun CACHE STRING "exective to be used in ctest")
set(TEST_ARG CACHE STRING "arguments to be used in ctest")

include_directories(./)
add_subdirectory(loopback)
add_subdirectory(pingpong)
