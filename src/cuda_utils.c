#include "cuda_utils.h"

int LCI_is_dev_ptr(const void* ptr)
{
    if (!ptr) return -1;
    cudaPointerAttributes attr;
    cudaError_t e = cudaPointerGetAttributes(&attr, ptr);

    return (e == cudaSuccess);
}