# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: NCSA

add_lci_tests(
  TESTS
  test_cxx11.cpp
  test_preinitialized_lct_pmi.cpp
  LABELS
  cxx11
  COMMANDS
  "[TARGET]")

add_lci_test(
  test-cxx11-test_lct_pmi_torchrun-multiprocess
  SOURCES
  test_lct_pmi_torchrun.cpp
  LABELS
  cxx11
  COMMANDS
  "${CMAKE_CURRENT_SOURCE_DIR}/run_torchrun_pmi_test.sh [TARGET] 2 pmi LCT"
  "${CMAKE_CURRENT_SOURCE_DIR}/run_torchrun_pmi_test.sh [TARGET] 2 pmi LCI"
  "${CMAKE_CURRENT_SOURCE_DIR}/run_torchrun_pmi_test.sh [TARGET] 2 pmi MASTER"
  "${CMAKE_CURRENT_SOURCE_DIR}/run_torchrun_pmi_test.sh [TARGET] 1 fallback-local NONE"
  "${CMAKE_CURRENT_SOURCE_DIR}/run_torchrun_pmi_test.sh [TARGET] 2 mixed-expect-fail MIXED"
)

add_test(
  NAME test-cxx11-test_lct_pmi_torchrun-no-std-thread
  COMMAND
    ${CMAKE_COMMAND}
    -DBACKEND_SOURCE=${CMAKE_SOURCE_DIR}/lct/pmi/pmi_wrapper_tcp.cpp
    -P ${CMAKE_CURRENT_SOURCE_DIR}/check_torchrun_no_std_thread.cmake)
set_property(
  TEST test-cxx11-test_lct_pmi_torchrun-no-std-thread
  PROPERTY LABELS cxx11)
set_target_properties(
  test-cxx11-test_cxx11
  PROPERTIES CXX_STANDARD 11
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF)
