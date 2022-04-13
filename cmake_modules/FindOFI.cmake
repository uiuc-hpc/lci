#[=======================================================================[.rst:
FindOFI
----------

Finds libfabric.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``OFI::OFI``
  The OpenFabrics Interfaces user-space library

Result Variables
^^^^^^^^^^^^^^^^

``OFI_FOUND``
  Set if libfabric was found

``OFI_INCLUDE_DIRS``
  Include directories needed to use libfabric

``OFI_LIBRARIES``
  Libraries needed to link to libfabric

``OFI_CFLAGS_OTHER``
  Other CFLAGS needed to use libfabric

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``OFI_INCLUDE_DIR``
  The include directory for libfabric

``OFI_LIBRARY``
  Path to the library for libfabric

#]=======================================================================]

include(FindPackageHandleStandardArgs)
find_package(PkgConfig)

pkg_check_modules(_OFI_PC QUIET libfabric)
find_path(OFI_INCLUDE_DIR
          NAMES "rdma/fabric.h"
          PATHS ${_OFI_PC_INCLUDE_DIRS}
          HINTS ENV OFI_ROOT
          PATH_SUFFIXES include
)
find_library(OFI_LIBRARY
             NAMES fabric
             PATHS ${_OFI_PC_LIBRARY_DIRS}
             HINTS ENV OFI_ROOT
             PATH_SUFFIXES lib
)
set(OFI_VERSION ${_OFI_PC_VERSION})

find_package_handle_standard_args(OFI
  REQUIRED_VARS
    OFI_INCLUDE_DIR
    OFI_LIBRARY
  VERSION_VAR OFI_VERSION
)

if(OFI_FOUND)
  set(OFI_INCLUDE_DIRS ${OFI_INCLUDE_DIR})
  set(OFI_LIBRARIES ${OFI_LIBRARY})
  set(OFI_CFLAGS_OTHER ${_OFI_PC_CFLAGS_OTHER})

  if(NOT TARGET OFI::OFI)
    add_library(OFI::OFI UNKNOWN IMPORTED)
    set_target_properties(OFI::OFI PROPERTIES
      IMPORTED_LOCATION "${OFI_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${OFI_INCLUDE_DIR}"
      INTERFACE_COMPILE_OPTIONS "${_OFI_PC_CFLAGS_OTHER}"
    )
  endif()

  mark_as_advanced(OFI_INCLUDE_DIR OFI_LIBRARY)
endif()
