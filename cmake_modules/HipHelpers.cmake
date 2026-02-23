# Copyright (c) 2026 The LCI Project Authors
# SPDX-License-Identifier: NCSA

include_guard(GLOBAL)

# Enable the HIP language and find the HIP/ROCm toolkit.
macro(enable_hip_language_and_find_hip)

  # Enable the HIP language
  enable_language(HIP)

  # Find the HIP/ROCm toolkit
  find_package(hip REQUIRED)

endmacro(enable_hip_language_and_find_hip)

function(infer_hip_standard STANDARD)
  if(CMAKE_HIP_STANDARD)
    # Use CMAKE_HIP_STANDARD if set
    set(${STANDARD}
        ${CMAKE_HIP_STANDARD}
        PARENT_SCOPE)
  elseif(CMAKE_CXX_STANDARD)
    # Otherwise use CMAKE_CXX_STANDARD
    set(${STANDARD}
        ${CMAKE_CXX_STANDARD}
        PARENT_SCOPE)
  endif()
endfunction(infer_hip_standard)

# If the variable supplied in ARCHS is empty, populate it with a list of common
# HIP architectures.
function(populate_hip_archs_list ARCHS)

  set(archs)

  if(CMAKE_HIP_ARCHITECTURES)
    set(archs ${CMAKE_HIP_ARCHITECTURES})
  elseif(NOT ("${${ARCHS}}" STREQUAL ""))
    # Support comma-delimited lists in addition to semicolons
    string(REPLACE "," ";" archs "${${ARCHS}}")
  else()
    # cmake-format: off
    # Default to common AMD GPU architectures
    # gfx900  - MI25
    # gfx906  - MI50/MI60
    # gfx908  - MI100
    # gfx90a  - MI210/MI250/MI250X
    # gfx942  - MI300A/MI300X/MI325X
    # gfx950  - MI350X/MI355X
    # cmake-format: on

    # Default to CDNA2 and CDNA3 architectures
    set(archs gfx90a gfx942)
  endif()

  list(REMOVE_DUPLICATES archs)

  set(${ARCHS}
      ${archs}
      PARENT_SCOPE)
endfunction(populate_hip_archs_list)

# Set a target's HIP_STANDARD property.
function(set_target_hip_standard hip_TARGET)
  set(options)
  set(oneValueArgs STANDARD)
  set(multiValueArgs)
  cmake_parse_arguments(hip "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  if(NOT ("${hip_STANDARD}" STREQUAL ""))
    set_target_properties(${hip_TARGET} PROPERTIES HIP_STANDARD ${hip_STANDARD}
                                                   HIP_STANDARD_REQUIRED ON)
  endif()
endfunction(set_target_hip_standard)

# Set a target's HIP_ARCHITECTURES property.
function(set_target_hip_architectures hip_TARGET)
  set(options)
  set(oneValueArgs)
  set(multiValueArgs ARCHITECTURES)
  cmake_parse_arguments(hip "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  if(hip_ARCHITECTURES)
    set_property(TARGET ${hip_TARGET} PROPERTY HIP_ARCHITECTURES
                                               ${hip_ARCHITECTURES})
  endif()
endfunction(set_target_hip_architectures)

# Set warning options on HIP targets.
function(set_target_hip_warnings_and_errors hip_TARGET)
  set(options)
  set(oneValueArgs WARN_AS_ERROR)
  set(multiValueArgs)
  cmake_parse_arguments(hip "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  # HIP/ROCm compiler warnings configuration
  if(hip_WARN_AS_ERROR)
    target_compile_options(${hip_TARGET}
                           PRIVATE $<$<COMPILE_LANG_AND_ID:HIP,Clang>:-Werror>)
  endif()
endfunction(set_target_hip_warnings_and_errors)
