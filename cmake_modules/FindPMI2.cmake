# Copyright (c)      2017 Thomas Heller
#
# SPDX-License-Identifier: BSL-1.0 Distributed under the Boost Software License,
# Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

find_package(PkgConfig QUIET)
# look for cray pmi... cray pmi has both pmi and pmi2
pkg_check_modules(PC_PMI_CRAY QUIET cray-pmi)
# look for the rest if we couldn't find the cray package
if(NOT PC_PMI_CRAY_FOUND)
  pkg_check_modules(PC_PMI2 QUIET pmi2)
endif()

find_path(
  PMI2_INCLUDE_DIR pmi2.h
  HINTS ${PMI2_ROOT}
        $ENV{PMI2_ROOT}
        ${PMI2_DIR}
        $ENV{PMI2_DIR}
        ${PC_PMI_CRAY_INCLUDEDIR}
        ${PC_PMI_CRAY_INCLUDE_DIRS}
        ${PC_PMI2_INCLUDEDIR}
        ${PC_PMI2_INCLUDE_DIRS}
        ENV
        CPATH
  PATH_SUFFIXES include slurm include/slurm)

find_library(
  PMI2_LIBRARY
  NAMES pmi2
  HINTS ${PMI2_ROOT}
        $ENV{PMI2_ROOT}
        ${PC_PMI_CRAY_LIBDIR}
        ${PC_PMI_CRAY_LIBRARY_DIRS}
        ${PC_PMI2_LIBDIR}
        ${PC_PMI2_LIBRARY_DIRS}
        ENV
        LD_LIBRARY_PATH
  PATH_SUFFIXES lib lib64 slurm lib/slurm lib64/slurm)

# Set PMI2_ROOT in case the other hints are used
if(PMI2_ROOT)
  # The call to file is for compatibility with windows paths
  file(TO_CMAKE_PATH ${PMI2_ROOT} PMI2_ROOT)
elseif("$ENV{PMI2_ROOT}")
  file(TO_CMAKE_PATH $ENV{PMI2_ROOT} PMI2_ROOT)
else()
  file(TO_CMAKE_PATH "${PMI2_INCLUDE_DIR}" PMI2_INCLUDE_DIR)
  string(REPLACE "/include" "" PMI2_ROOT "${PMI2_INCLUDE_DIR}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PMI2 DEFAULT_MSG PMI2_LIBRARY
                                  PMI2_INCLUDE_DIR)

mark_as_advanced(PMI2_ROOT PMI2_LIBRARY PMI2_INCLUDE_DIR)

add_library(PMI2::PMI2 INTERFACE IMPORTED)
target_include_directories(PMI2::PMI2 SYSTEM INTERFACE ${PMI2_INCLUDE_DIR})
target_link_libraries(PMI2::PMI2 INTERFACE ${PMI2_LIBRARY})
