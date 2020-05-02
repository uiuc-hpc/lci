#ifndef CUDA_UTILS_H_
#define CUDA_UTILS_H_

#include <cuda_runtime_api.h>

#include "macro.h"

/* returns 1 if ptr is not a host pointer (i.e. can refer to device memory) */
LC_INLINE
int lc_is_dev_ptr(const void *ptr)
{
  struct cudaPointerAttributes attr;
  cudaError_t e = cudaPointerGetAttributes(&attr, ptr);
  return (e == cudaSuccess)                &&
         (attr.type != cudaMemoryTypeHost) &&
         (attr.type != cudaMemoryTypeUnregistered);
}

#endif // CUDA_UTILS_H_
