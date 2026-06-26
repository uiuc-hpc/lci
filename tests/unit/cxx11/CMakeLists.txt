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
set_target_properties(
  test-cxx11-test_cxx11
  PROPERTIES CXX_STANDARD 11
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF)
