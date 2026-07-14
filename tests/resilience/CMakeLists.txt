# Copyright (c) 2026 The LCI Project Authors
# SPDX-License-Identifier: NCSA

add_lci_test(
  test-resilience-process-failure
  SOURCES
  process_failure.cpp
  LABELS
  resilience
  COMMANDS
  "${CMAKE_CURRENT_SOURCE_DIR}/run_process_failure_test.sh [TARGET]"
  INCLUDES
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/lct/api
  ${PROJECT_BINARY_DIR}/lct/api
  ${OFI_INCLUDE_DIRS})
