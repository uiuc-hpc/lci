# Copyright (c)      2017 Thomas Heller
#
# SPDX-License-Identifier: BSL-1.0 Distributed under the Boost Software License,
# Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

find_package(PkgConfig QUIET)

pkg_check_modules(PC_PMIX QUIET pmix)

find_path(
  PMIX_INCLUDE_DIR pmix.h
  HINTS ${PMIX_ROOT} $ENV{PMIX_ROOT} ${PMIX_DIR} $ENV{PMIX_DIR}
        ${PC_PMIX_INCLUDEDIR} ${PC_PMIX_INCLUDE_DIRS}
  PATH_SUFFIXES include)

find_library(
  PMIX_LIBRARY
  NAMES pmix
  HINTS ${PMIX_ROOT} $ENV{PMIX_ROOT} ${PC_PMIX_LIBDIR} ${PC_PMIX_LIBRARY_DIRS}
  PATH_SUFFIXES lib lib64)

# Set PMIX_ROOT in case the other hints are used
if(PMIX_ROOT)
  # The call to file is for compatibility with windows paths
  file(TO_CMAKE_PATH ${PMIX_ROOT} PMIX_ROOT)
elseif("$ENV{PMIX_ROOT}")
  file(TO_CMAKE_PATH $ENV{PMIX_ROOT} PMIX_ROOT)
else()
  file(TO_CMAKE_PATH "${PMIX_INCLUDE_DIR}" PMIX_INCLUDE_DIR)
  string(REPLACE "/include" "" PMIX_ROOT "${PMIX_INCLUDE_DIR}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PMIx DEFAULT_MSG PMIX_LIBRARY
                                  PMIX_INCLUDE_DIR)

mark_as_advanced(PMIX_ROOT PMIX_LIBRARY PMIX_INCLUDE_DIR)

add_library(PMIx::PMIx INTERFACE IMPORTED)
target_include_directories(PMIx::PMIx SYSTEM INTERFACE ${PMIX_INCLUDE_DIR})
target_link_libraries(PMIx::PMIx INTERFACE ${PMIX_LIBRARY})
