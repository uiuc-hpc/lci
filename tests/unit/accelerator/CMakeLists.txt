# Copyright (c) 2025-2026 The LCI Project Authors
# SPDX-License-Identifier: NCSA

if(LCI_USE_CUDA)
  add_lci_tests(
    TESTS
    test_gpu_comm.cu
    LABELS
    accelerator
    COMMANDS
    "${LCI_USE_CTEST_LAUNCHER} -n 1 ${LCI_USE_CTEST_ARGS} [TARGET]"
    "${LCI_USE_CTEST_LAUNCHER} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET]"
    DEPENDENCIES
    CUDA::cudart)
  set_target_cuda_standard(test-accelerator-test_gpu_comm STANDARD
                           ${LCI_CUDA_STANDARD})
  set_target_cuda_architectures(test-accelerator-test_gpu_comm ARCHITECTURES
                                ${LCI_CUDA_ARCH})
  set_target_cuda_warnings_and_errors(test-accelerator-test_gpu_comm
                                      WARN_AS_ERROR ${LCI_BUILD_WARN_AS_ERROR})
endif()

if(LCI_USE_HIP)
  add_lci_tests(
    TESTS
    test_gpu_comm_hip.hip
    LABELS
    accelerator
    COMMANDS
    "${LCI_USE_CTEST_LAUNCHER} -n 1 ${LCI_USE_CTEST_ARGS} [TARGET]"
    "${LCI_USE_CTEST_LAUNCHER} -n 2 ${LCI_USE_CTEST_ARGS} [TARGET]"
    DEPENDENCIES
    hip::host)
  set_target_hip_standard(test-accelerator-test_gpu_comm_hip STANDARD
                          ${LCI_HIP_STANDARD})
  set_target_hip_architectures(test-accelerator-test_gpu_comm_hip ARCHITECTURES
                               ${LCI_HIP_ARCH})
  set_target_hip_warnings_and_errors(test-accelerator-test_gpu_comm_hip
                                     WARN_AS_ERROR ${LCI_BUILD_WARN_AS_ERROR})
endif()
