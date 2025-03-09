# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: MIT

# Execute generate_binding.py
find_package(Python COMPONENTS Interpreter)
if(NOT Python_EXECUTABLE)
  message(WARNING "Python executable not found")
else()
  execute_process(
    COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/generate_binding.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE py_result
    OUTPUT_VARIABLE py_output
    ERROR_VARIABLE py_error
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE)

  # Check if the command failed
  if(NOT py_result EQUAL 0)
    message(
      WARNING
        "Python script generate_binding.py failed with error code: ${py_result}"
    )
    message(WARNING "Error message: ${py_error}")
  endif()
endif()

target_include_directories(
  LCI PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/generated>
             $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
