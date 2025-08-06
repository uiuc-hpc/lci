# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: NCSA

add_lci_tests(TESTS test_cxx11.cpp LABELS cxx11 COMMANDS "[TARGET]")
set_target_properties(
  test-cxx11-test_cxx11
  PROPERTIES CXX_STANDARD 11
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF)
