# Copyright (c) 2026 The LCI Project Authors
# SPDX-License-Identifier: NCSA

get_target_property(LCI_LINKED_LIBS LCI LINK_LIBRARIES)

set(LCI_ATOMIC_TEST_DEPENDENCIES ${LCI_LINKED_LIBS})
if(LCI_USE_HIP)
  set_source_files_properties(test_network_atomic.cpp PROPERTIES LANGUAGE HIP)
  list(APPEND LCI_ATOMIC_TEST_DEPENDENCIES hip::host)
endif()

if(LCI_BACKEND_ENABLE_OFI)
  add_lci_test(
    test-atomic-network-unsupported
    SOURCES
    test_network_atomic.cpp
    LABELS
    atomic
    ofi
    COMMANDS
    "env LCI_ATTR_BACKEND=ofi [TARGET] --expect-unsupported"
    INCLUDES
    ${PROJECT_SOURCE_DIR}/src
    DEPENDENCIES
    ${LCI_ATOMIC_TEST_DEPENDENCIES})
endif()

if(LCI_BACKEND_ENABLE_IBV)
  set(LCI_ATOMIC_IBV_COMMANDS
      "${CMAKE_CURRENT_SOURCE_DIR}/run_ibv_atomic_test.sh ${LCI_USE_CTEST_LAUNCHER} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --require-ibv"
  )
  if(LCI_USE_HIP)
    list(
      APPEND
      LCI_ATOMIC_IBV_COMMANDS
      "${CMAKE_CURRENT_SOURCE_DIR}/run_ibv_atomic_test.sh ${LCI_USE_CTEST_LAUNCHER} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --require-ibv-gpu"
    )
  endif()
  add_lci_test(
    test-atomic-network-ibv
    SOURCES
    test_network_atomic.cpp
    LABELS
    atomic
    ibv
    hardware
    COMMANDS
    ${LCI_ATOMIC_IBV_COMMANDS}
    INCLUDES
    ${PROJECT_SOURCE_DIR}/src
    DEPENDENCIES
    ${LCI_ATOMIC_TEST_DEPENDENCIES})
  set_tests_properties(test-atomic-network-ibv PROPERTIES SKIP_RETURN_CODE 77)
  if(LCI_USE_HIP)
    set_tests_properties(test-atomic-network-ibv-1 PROPERTIES SKIP_RETURN_CODE
                                                              77)
  endif()
endif()

if(LCI_USE_HIP)
  foreach(target test-atomic-network-unsupported test-atomic-network-ibv)
    if(TARGET ${target})
      set_target_hip_standard(${target} STANDARD ${LCI_HIP_STANDARD})
      set_target_hip_architectures(${target} ARCHITECTURES ${LCI_HIP_ARCH})
      set_target_hip_warnings_and_errors(${target} WARN_AS_ERROR
                                         ${LCI_BUILD_WARN_AS_ERROR})
    endif()
  endforeach()
endif()
