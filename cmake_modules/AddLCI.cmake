# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: MIT

function(add_lci_executable name)
  add_executable(${name} ${ARGN})
  target_compile_definitions(${name} PRIVATE _GNU_SOURCE)
  target_link_libraries(${name} PRIVATE LCI)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.13")
    target_link_options(${name} PRIVATE LINKER:-z,now LINKER:-z,relro)
  else()
    set_property(
      TARGET ${name}
      APPEND_STRING
      PROPERTY LINK_FLAGS " -Wl,-z,now -Wl,-z,relro")
  endif()
  set_target_properties(
    ${name}
    PROPERTIES C_STANDARD 99
               C_EXTENSIONS ON
               CXX_STANDARD 11)
  set_target_properties(${name} PROPERTIES OUTPUT_NAME "lci_${name}")
  install(TARGETS ${name} RUNTIME)
endfunction()

function(add_lci_test name)
  cmake_parse_arguments(
    ARG "" "" "COMMANDS;LABELS;SOURCES;INCLUDES;DEPENDENCIES;ENVIRONMENT"
    ${ARGN})

  add_lci_executable(${name} ${ARG_SOURCES})
  target_include_directories(${name} PRIVATE ${ARG_INCLUDES})
  target_link_libraries(${name} PRIVATE ${ARG_DEPENDENCIES})

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
    if(ENVIRONMENT)
      set_tests_properties(${test_name} PROPERTIES ENVIRONMENT ${ENVIRONMENT})
    endif()
  endwhile()
endfunction()

function(add_lci_tests)
  cmake_parse_arguments(
    ARG "" "" "COMMANDS;LABELS;TESTS;INCLUDES;DEPENDENCIES;ENVIRONMENT" ${ARGN})
  foreach(name ${ARG_TESTS})
    string(REGEX REPLACE "\\.[^.]*$" "" name_without_ext ${name})
    add_lci_test(
      test-${ARG_LABELS}-${name_without_ext}
      SOURCES
      ${name}
      LABELS
      ${ARG_LABELS}
      COMMANDS
      ${ARG_COMMANDS}
      INCLUDES
      ${ARG_INCLUDES}
      DEPENDENCIES
      ${ARG_DEPENDENCIES}
      ENVIRONMENT
      ${ENVIRONMENT})
  endforeach()
endfunction()
