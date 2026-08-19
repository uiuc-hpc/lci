# Copyright (c) 2026 The LCI Project Authors
# SPDX-License-Identifier: NCSA

get_target_property(LCI_LINKED_LIBS LCI LINK_LIBRARIES)

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
    ${LCI_LINKED_LIBS})
endif()

if(LCI_BACKEND_ENABLE_IBV)
  add_lci_test(
    test-atomic-network-ibv
    SOURCES
    test_network_atomic.cpp
    LABELS
    atomic
    ibv
    hardware
    COMMANDS
    "${CMAKE_CURRENT_SOURCE_DIR}/run_ibv_atomic_test.sh ${LCI_USE_CTEST_LAUNCHER} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET] --require-ibv"
    INCLUDES
    ${PROJECT_SOURCE_DIR}/src
    DEPENDENCIES
    ${LCI_LINKED_LIBS})
  set_tests_properties(test-atomic-network-ibv PROPERTIES SKIP_RETURN_CODE 77)
endif()
