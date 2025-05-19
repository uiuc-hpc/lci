# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: MIT

add_lci_tests(
  TESTS
  all.cpp
  LABELS
  loopback
  COMMANDS
  "[TARGET]"
  INCLUDES
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/tests/unit/include
  DEPENDENCIES
  gtest_main)
