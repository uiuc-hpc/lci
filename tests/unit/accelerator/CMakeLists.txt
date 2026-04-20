# Copyright (c) 2025-2026 The LCI Project Authors
# SPDX-License-Identifier: NCSA

if(LCI_USE_CUDA)
  add_lci_tests(
    TESTS
    test_gpu_comm.cu
    test_gpu_mem_reg_stress.cu
    LABELS
    accelerator
    COMMANDS
    "${LCI_USE_CTEST_LAUNCHER} -n 1 ${LCI_USE_CTEST_ARGS} [TARGET]"
    "${LCI_USE_CTEST_LAUNCHER} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET]"
    DEPENDENCIES
    CUDA::cudart)
  foreach(target test-accelerator-test_gpu_comm
                 test-accelerator-test_gpu_mem_reg_stress)
    set_target_cuda_standard(${target} STANDARD ${LCI_CUDA_STANDARD})
    set_target_cuda_architectures(${target} ARCHITECTURES ${LCI_CUDA_ARCH})
    set_target_cuda_warnings_and_errors(${target} WARN_AS_ERROR
                                        ${LCI_BUILD_WARN_AS_ERROR})
  endforeach()
endif()

if(LCI_USE_HIP)
  set(LCI_ACCELERATOR_HIP_TESTS test_gpu_comm.hip.cpp
                                test_gpu_mem_reg_stress.hip.cpp)
  set_source_files_properties(${LCI_ACCELERATOR_HIP_TESTS} PROPERTIES LANGUAGE
                                                                      HIP)
  add_lci_tests(
    TESTS
    ${LCI_ACCELERATOR_HIP_TESTS}
    LABELS
    accelerator
    COMMANDS
    "${LCI_USE_CTEST_LAUNCHER} -n 1 ${LCI_USE_CTEST_ARGS} [TARGET]"
    "${LCI_USE_CTEST_LAUNCHER} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET]"
    DEPENDENCIES
    hip::host)
  foreach(target test-accelerator-test_gpu_comm
                 test-accelerator-test_gpu_mem_reg_stress)
    set_target_hip_standard(${target} STANDARD ${LCI_HIP_STANDARD})
    set_target_hip_architectures(${target} ARCHITECTURES ${LCI_HIP_ARCH})
    set_target_hip_warnings_and_errors(${target} WARN_AS_ERROR
                                       ${LCI_BUILD_WARN_AS_ERROR})
  endforeach()
endif()
