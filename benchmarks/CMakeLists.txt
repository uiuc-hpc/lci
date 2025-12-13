# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: NCSA

function(add_lci_benchmarks)
  cmake_parse_arguments(ARG "" "" "INCLUDES;DEPENDENCIES" ${ARGN})

  set(SRCS ${ARG_UNPARSED_ARGUMENTS})
  foreach(name ${SRCS})
    string(REGEX REPLACE "\\.[^.]*$" "" name_without_ext ${name})
    add_lci_executable(${name_without_ext} ${name})
    if(ARG_INCLUDES)
      target_include_directories(${name_without_ext} PRIVATE ${ARG_INCLUDES})
    endif()
    if(ARG_DEPENDENCIES)
      target_link_libraries(${name_without_ext} PRIVATE ${ARG_DEPENDENCIES})
    endif()
  endforeach()

  add_lci_tests(
    TESTS
    ${SRCS}
    LABELS
    benchmark
    INCLUDES
    ${ARG_INCLUDES}
    DEPENDENCIES
    ${ARG_DEPENDENCIES}
    COMMANDS
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
find_package(OpenMP)

add_lci_benchmarks(bench_collective.cpp DEPENDENCIES cxxopts::cxxopts)

if(OpenMP_FOUND)
  add_lci_benchmarks(bench_many2many_am.cpp DEPENDENCIES OpenMP::OpenMP_CXX
                     cxxopts::cxxopts)
else()
  message(WARNING "OpenMP not found. Skipping many2many_am benchmark.")
endif()

add_subdirectory(resources)
add_subdirectory(pt2pt)
