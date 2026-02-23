# Copyright (c) 2025-2026 The LCI Project Authors
# SPDX-License-Identifier: NCSA

if(LCI_USE_CUDA)
  target_sources_relative(LCI PRIVATE accelerator_cuda.cpp)
endif()

if(LCI_USE_HIP)
  target_sources_relative(LCI PRIVATE accelerator_hip.cpp)
endif()
