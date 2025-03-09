# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: MIT

find_package(OpenMP REQUIRED)

option(LCI_BUILD_TESTS "Build tests by default" ON)
if(NOT LCI_BUILD_TESTS)
  set(EXCLUDE_FROM_ALL ON)
endif()

include_directories(./)
add_subdirectory(unit)
