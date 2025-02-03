# Execute generate_binding.py
execute_process(
  COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/generate_binding.py
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  RESULT_VARIABLE py_result)
# message(STATUS "Generate binding: ${py_result}")

target_include_directories(
  LCI PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/generated>
             $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
