# Copyright (c)      2017 Thomas Heller
#
# SPDX-License-Identifier: BSL-1.0 Distributed under the Boost Software License,
# Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

find_package(PkgConfig QUIET)
# look for cray pmi...
pkg_check_modules(PC_PMI_CRAY QUIET cray-pmi)
# look for the rest if we couldn't find the cray package
if(NOT PC_PMI_CRAY_FOUND)
  pkg_check_modules(PC_PMI QUIET pmi)
endif()

find_path(
  PMI2_INCLUDE_DIR pmi2.h
  HINTS ${PMI2_ROOT}
        $ENV{PMI2_ROOT}
        ${PMI2_DIR}
        $ENV{PMI2_DIR}
        ${PC_PMI_CRAY_INCLUDEDIR}
        ${PC_PMI_CRAY_INCLUDE_DIRS}
        ${PC_PMI_INCLUDEDIR}
        ${PC_PMI_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/src/pmi/pmi2/
  PATH_SUFFIXES include)

find_library(
  PMI2_LIBRARY
  NAMES pmi2 pmi
  HINTS ${PMI2_ROOT} $ENV{PMI2_ROOT} ${PC_PMI_CRAY_LIBDIR}
        ${PC_PMI_CRAY_LIBRARY_DIRS} ${PC_PMI_LIBDIR} ${PC_PMI_LIBRARY_DIRS}
  PATH_SUFFIXES lib lib64)

# Set PMI2_ROOT in case the other hints are used
if(PMI2_ROOT)
  # The call to file is for compatibility with windows paths
  file(TO_CMAKE_PATH ${PMI2_ROOT} PMI2_ROOT)
elseif("$ENV{PMI2_ROOT}")
  file(TO_CMAKE_PATH $ENV{PMI2_ROOT} PMI2_ROOT)
else()
  file(TO_CMAKE_PATH "${PMI2_INCLUDE_DIR}" PMI2_INCLUDE_DIR)
  string(REPLACE "/src/api" "" PMI2_ROOT "${PMI2_INCLUDE_DIR}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PMI2 DEFAULT_MSG PMI2_LIBRARY
                                  PMI2_INCLUDE_DIR)

mark_as_advanced(PMI2_ROOT PMI2_LIBRARY PMI2_INCLUDE_DIR)

add_library(PMI::PMI2 INTERFACE IMPORTED)
target_include_directories(PMI::PMI2 SYSTEM INTERFACE ${PMI2_INCLUDE_DIR})
target_link_libraries(PMI::PMI2 INTERFACE ${PMI2_LIBRARY})
