# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: MIT

#[=======================================================================[.rst:
FindIBV
----------

Finds libibverbs.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``IBV::IBV``
  The InfiniBand Verbs library

Result Variables
^^^^^^^^^^^^^^^^

``IBV_FOUND``
  Set if libibverbs was found

``IBV_INCLUDE_DIRS``
  Include directories needed to use libibverbs

``IBV_LIBRARIES``
  Libraries needed to link to libibverbs

``IBV_CFLAGS_OTHER``
  Other CFLAGS needed to use libibverbs

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``IBV_INCLUDE_DIR``
  The include directory for libibverbs

``IBV_LIBRARY``
  Path to the library for libibverbs

#]=======================================================================]

include(FindPackageHandleStandardArgs)
find_package(PkgConfig)

pkg_check_modules(_IBV_PC QUIET libibverbs)
find_path(
  IBV_INCLUDE_DIR
  NAMES "infiniband/verbs.h"
  PATHS ${_IBV_PC_INCLUDE_DIRS}
  HINTS $ENV{IBV_ROOT} $ENV{LIBIBVERBS_ROOT}
  PATH_SUFFIXES include)
find_library(
  IBV_LIBRARY
  NAMES ibverbs
  PATHS ${_IBV_PC_LIBRARY_DIRS}
  HINTS $ENV{IBV_ROOT} $ENV{LIBIBVERBS_ROOT}
  PATH_SUFFIXES lib)
set(IBV_VERSION ${_IBV_PC_VERSION})

find_package_handle_standard_args(
  IBV
  REQUIRED_VARS IBV_INCLUDE_DIR IBV_LIBRARY
  VERSION_VAR IBV_VERSION)

if(IBV_FOUND)
  set(IBV_INCLUDE_DIRS ${IBV_INCLUDE_DIR})
  set(IBV_LIBRARIES ${IBV_LIBRARY})
  set(IBV_CFLAGS_OTHER ${_IBV_PC_CFLAGS_OTHER})
  if(_IBV_PC_FOUND)
    set(Fabric_IBV_PC_Requires libibverbs)
  else()
    set(Fabric_IBV_PC_Libs "-libverbs")
  endif()

  if(NOT TARGET IBV::IBV)
    add_library(IBV::IBV UNKNOWN IMPORTED)
    set_target_properties(
      IBV::IBV
      PROPERTIES IMPORTED_LOCATION "${IBV_LIBRARY}"
                 INTERFACE_INCLUDE_DIRECTORIES "${IBV_INCLUDE_DIR}"
                 INTERFACE_COMPILE_OPTIONS "${_IBV_PC_CFLAGS_OTHER}")
  endif()

  mark_as_advanced(IBV_INCLUDE_DIR IBV_LIBRARY)
endif()
