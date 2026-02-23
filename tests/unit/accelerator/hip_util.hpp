#include <hip/hip_runtime.h>

#define HIP_CHECK(call)                                                \
  do {                                                                 \
    hipError_t err = call;                                             \
    if (hipSuccess != err) {                                           \
      fprintf(stderr, "HIP failure %s:%d: '%s'\n", __FILE__, __LINE__, \
              hipGetErrorString(err));                                 \
      exit(EXIT_FAILURE);                                              \
    }                                                                  \
  } while (0)
