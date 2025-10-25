# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: NCSA

function(add_lci_benchmarks)
  foreach(name ${ARGN})
    string(REGEX REPLACE "\\.[^.]*$" "" name_without_ext ${name})
    add_lci_executable(${name_without_ext} ${name})
  endforeach()
  add_lci_tests(TESTS ${ARGN} LABELS benchmark COMMANDS
                "${LCI_USE_CTEST_LAUNCHER} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET]")
endfunction()

include(FetchContent)
if(NOT TARGET cxxopts::cxxopts)
  FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG v3.3.1 # latest as of June 2025
  )
  FetchContent_MakeAvailable(cxxopts)
endif()

option(LCI_BUILD_BENCHMARKS "Build benchmarks by default" ON)
if(NOT LCI_BUILD_BENCHMARKS)
  set(EXCLUDE_FROM_ALL ON)
endif()
add_lci_benchmarks(collective.cpp)
target_link_libraries(collective PRIVATE cxxopts::cxxopts)
if(TARGET test-benchmark-collective)
  target_link_libraries(test-benchmark-collective PRIVATE cxxopts::cxxopts)
endif()

add_subdirectory(resources)
add_subdirectory(pt2pt)
