# Copyright (c) 2025-2026 The LCI Project Authors
# SPDX-License-Identifier: NCSA

if(LCI_USE_CUDA)
  target_sources_relative(LCI PRIVATE accelerator_cuda.cpp)
endif()

if(LCI_USE_HIP)
  target_sources_relative(LCI PRIVATE accelerator_hip.cpp)
  target_link_libraries(LCI PRIVATE hip::host hsa-runtime64::hsa-runtime64)

  # ROCm HIP headers rely on anonymous struct/union extensions that trigger
  # pedantic warnings with clang-family compilers.
  target_compile_options(
    LCI
    PRIVATE
      $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:CrayClang>>>:-Wno-gnu-anonymous-struct>
      $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:CrayClang>>>:-Wno-nested-anon-types>
  )
endif()
