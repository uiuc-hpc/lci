# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: NCSA

# We need to link to all the libraries that LCI is linked to because we use LCI
# internal API in the test.
get_target_property(LCI_LINKED_LIBS LCI LINK_LIBRARIES)

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
  gtest_main
  ${LCI_LINKED_LIBS})
