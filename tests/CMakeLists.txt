find_package(OpenMP REQUIRED)

function(add_lci_test name)
  cmake_parse_arguments(ARG "" "" "COMMANDS;LABELS;SOURCES" ${ARGN})

  add_lci_executable(${name} ${ARG_SOURCES})
  target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
  target_link_libraries(${name} PRIVATE OpenMP::OpenMP_CXX)

  # string(REGEX REPLACE "\\[TARGET\\]" $<TARGET_FILE:${name}> TEST_COMMAND
  # ${ARG_COMMAND}) string(REPLACE " " ";" TEST_COMMAND ${TEST_COMMAND})
  # add_test(NAME ${name} COMMAND ${TEST_COMMAND}) set_property(TEST ${name}
  # PROPERTY LABELS ${ARG_LABELS})

  list(LENGTH ARG_COMMANDS count)
  set(index 0)
  while(index LESS count)
    set(test_name ${name})
    if(index GREATER 0)
      set(test_name ${name}-${index})
    endif()
    list(GET ARG_COMMANDS ${index} COMMAND)
    math(EXPR index "${index}+1")
    # set test
    string(REGEX REPLACE "\\[TARGET\\]" $<TARGET_FILE:${name}> TEST_COMMAND
                         ${COMMAND})
    string(REPLACE " " ";" TEST_COMMAND ${TEST_COMMAND})
    add_test(NAME ${test_name} COMMAND ${TEST_COMMAND})
    set_property(TEST ${test_name} PROPERTY LABELS ${ARG_LABELS})
  endwhile()
endfunction()

function(add_lci_tests)
  cmake_parse_arguments(ARG "" "" "COMMANDS;LABELS;TESTS" ${ARGN})
  foreach(name ${ARG_TESTS})
    string(REGEX REPLACE "\\.[^.]*$" "" name_without_ext ${name})
    add_lci_test(
      test-${ARG_LABELS}-${name_without_ext}
      SOURCES
      ${name}
      LABELS
      ${ARG_LABELS}
      COMMANDS
      ${ARG_COMMANDS})
  endforeach()
endfunction()

option(LCI_BUILD_TESTS "Build tests by default" ON)
if(NOT LCI_BUILD_TESTS)
  set(EXCLUDE_FROM_ALL ON)
endif()
set(SRUN_EXE
    mpirun
    CACHE STRING "exective to be used in ctest")
set(SRUN_EXTRA_ARG
    ""
    CACHE STRING "arguments to be used in ctest")

include_directories(./)
# add_subdirectory(gtests)
add_subdirectory(loopback)
add_subdirectory(pingpong)
add_subdirectory(lcit)
