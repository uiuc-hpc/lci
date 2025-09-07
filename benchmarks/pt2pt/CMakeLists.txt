# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: NCSA

# add cxxopts
include(FetchContent)
FetchContent_Declare(
  cxxopts
  GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
  GIT_TAG v3.3.1 # latest as of June 2025
)
FetchContent_MakeAvailable(cxxopts)

find_package(OpenMP)

if(NOT OpenMP_FOUND)
  message(WARNING "OpenMP not found. Skipping benchmark build.")
  return()
endif()

add_lci_executable(bench_put_bw bench_put_bw.cpp)
target_link_libraries(bench_put_bw PRIVATE OpenMP::OpenMP_CXX cxxopts::cxxopts)
add_lci_tests(
  TESTS
  bench_put_bw.cpp
  LABELS
  benchmark
  DEPENDENCIES
  OpenMP::OpenMP_CXX
  cxxopts::cxxopts
  COMMANDS
  "${LCI_USE_CTEST_LAUNCHER} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] -t 2 --nelems 1024 --niters 1"
)
