# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: MIT

#[=======================================================================[.rst:
FindPSM2
----------

Finds libpsm2.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``PSM2::PSM2``
  The Intel Performance Scaled Messaging 2 library

Result Variables
^^^^^^^^^^^^^^^^

``PSM2_FOUND``
  Set if libpsm2 was found

``PSM2_INCLUDE_DIRS``
  Include directories needed to use libpsm2

``PSM2_LIBRARIES``
  Libraries needed to link to libpsm2

``PSM2_CFLAGS_OTHER``
  Other CFLAGS needed to use libpsm2

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``PSM2_INCLUDE_DIR``
  The include directory for libpsm2

``PSM2_LIBRARY``
  Path to the library for libpsm2

#]=======================================================================]

include(FindPackageHandleStandardArgs)
find_package(PkgConfig)

pkg_check_modules(_PSM2_PC QUIET libpsm2)
find_path(
  PSM2_INCLUDE_DIR
  NAMES "psm2.h"
  PATHS ${_PSM2_PC_INCLUDE_DIRS}
  HINTS $ENV{PSM2_ROOT}
  PATH_SUFFIXES include)
find_library(
  PSM2_LIBRARY
  NAMES psm2
  PATHS ${_PSM2_PC_LIBRARY_DIRS}
  HINTS $ENV{PSM2_ROOT}
  PATH_SUFFIXES lib)
set(PSM2_VERSION ${_PSM2_PC_VERSION})

find_package_handle_standard_args(
  PSM2
  REQUIRED_VARS PSM2_INCLUDE_DIR PSM2_LIBRARY
  VERSION_VAR PSM2_VERSION)

if(PSM2_FOUND)
  set(PSM2_INCLUDE_DIRS ${PSM2_INCLUDE_DIR})
  set(PSM2_LIBRARIES ${PSM2_LIBRARY})
  set(PSM2_CFLAGS_OTHER ${_PSM2_PC_CFLAGS_OTHER})

  if(NOT TARGET PSM2::PSM2)
    add_library(PSM2::PSM2 UNKNOWN IMPORTED)
    set_target_properties(
      PSM2::PSM2
      PROPERTIES IMPORTED_LOCATION "${PSM2_LIBRARY}"
                 INTERFACE_INCLUDE_DIRECTORIES "${PSM2_INCLUDE_DIR}"
                 INTERFACE_COMPILE_OPTIONS "${_PSM2_PC_CFLAGS_OTHER}")
  endif()

  mark_as_advanced(PSM2_INCLUDE_DIR PSM2_LIBRARY)
endif()
