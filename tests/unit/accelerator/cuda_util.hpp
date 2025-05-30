#include <cuda_runtime.h>

#define CUDA_CHECK(call)                                                \
  do {                                                                  \
    cudaError_t err = call;                                             \
    if (cudaSuccess != err) {                                           \
      fprintf(stderr, "Cuda failure %s:%d: '%s'\n", __FILE__, __LINE__, \
              cudaGetErrorString(err));                                 \
      exit(EXIT_FAILURE);                                               \
    }                                                                   \
  } while (0)
