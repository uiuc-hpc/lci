# Copyright (c) 2025 The LCI Project Authors
# SPDX-License-Identifier: MIT

if(NOT LCI_USE_CUDA)
  return()
endif()

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
