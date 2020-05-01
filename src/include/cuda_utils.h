#ifndef CUDA_UTILS_H_
#define CUDA_UTILS_H_


#include <cuda.h>
#include "config.h"
#include "macro.h"

#ifdef LCI_CUDA

/*
    returns 1 if the buffer referenced by ptr is allocated on the device side
*/

LC_INLINE
int LCI_is_dev_ptr(const void* ptr)
{
    if (!ptr) return -1;
    cudaPointerAttributes attr;
    cudaError_t e = cudaPointerGetAttributes(&attr, ptr);

    return (e == cudaSuccess);
}


#endif // LCI_CUDA

#endif // CUDA_UTILS_H_