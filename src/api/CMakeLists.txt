# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: MIT

configure_file(lci_config.hpp.in lci_config.hpp @ONLY)
target_include_directories(
  LCI
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
