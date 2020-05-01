#ifndef CUDA_UTILS_H_
#define CUDA_UTILS_H_


#include <cuda.h>
#include "config.h"

#ifdef LCI_CUDA


int LCI_is_dev_ptr(const void* ptr);


#endif // LCI_CUDA

#endif // CUDA_UTILS_H_