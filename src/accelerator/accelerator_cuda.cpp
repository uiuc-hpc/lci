// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include <cuda.h>
#include "lci_internal.hpp"

#define CUDA_CHECK(call)                                                \
  do {                                                                  \
    cudaError_t err = call;                                             \
    if (cudaSuccess != err) {                                           \
      fprintf(stderr, "Cuda failure %s:%d: '%s'\n", __FILE__, __LINE__, \
              cudaGetErrorString(err));                                 \
      exit(EXIT_FAILURE);                                               \
    }                                                                   \
  } while (0)

#define CU_CHECK(call)                                                  \
  do {                                                                  \
    CUresult result = call;                                             \
    if (CUDA_SUCCESS != result) {                                       \
      fprintf(stderr, "CUDA failure %s:%d: '%s'\n", __FILE__, __LINE__, \
              #call);                                                   \
      exit(EXIT_FAILURE);                                               \
    }                                                                   \
  } while (0)

namespace lci
{
namespace accelerator
{
namespace details
{
std::vector<CUdevice> devices;

int get_device_from_context(CUcontext ctx)
{
  int ret = -1;
  CUdevice dev;
  CU_CHECK(cuCtxPushCurrent(ctx));
  CU_CHECK(cuCtxGetDevice(&dev));
  for (int i = 0; i < static_cast<int>(devices.size()); i++) {
    if (devices[i] == dev) {
      ret = i;
      break;
    }
  }
  CU_CHECK(cuCtxPopCurrent(NULL));
  return ret;
}
}  // namespace details

void initialize()
{
  CU_CHECK(cuInit(0));
  int num_devices;
  CU_CHECK(cuDeviceGetCount(&num_devices));
  for (int i = 0; i < num_devices; i++) {
    CUdevice dev;
    CU_CHECK(cuDeviceGet(&dev, i));
    char name[256];
    CU_CHECK(cuDeviceGetName(name, 256, dev));
    LCI_Log(LOG_INFO, "cuda", "Found CUDA device %d: %s\n", dev, name);
    details::devices.push_back(dev);
  }
}

void finalize() {}

buffer_attr_t get_buffer_attr(const void* ptr)
{
  buffer_attr_t attr_ret;
  memset(&attr_ret, 0, sizeof(attr_ret));
  CUmemorytype mem_type;
  CUcontext mem_ctx = 0;
  uint32_t is_managed = 0;
  CUpointer_attribute attributes[3] = {CU_POINTER_ATTRIBUTE_MEMORY_TYPE,
                                       CU_POINTER_ATTRIBUTE_CONTEXT,
                                       CU_POINTER_ATTRIBUTE_IS_MANAGED};
  void* attrdata[] = {(void*)&mem_type, (void*)&mem_ctx, (void*)&is_managed};
  CUresult result =
      cuPointerGetAttributes(3, attributes, attrdata, (CUdeviceptr)ptr);
  LCI_Log(LOG_TRACE, "cuda",
          "get_buffer_attr(&p) -> mem_type: %d, mem_ctx: %p, is_managed: %u\n",
          mem_type, mem_ctx, is_managed);
  LCI_Assert(!is_managed, "Managed memory is not supported for now");
  if (CUDA_SUCCESS != result) {
    if (CUDA_ERROR_NOT_INITIALIZED == result ||
        CUDA_ERROR_INVALID_VALUE == result) {
      // Unregistered host memory
      attr_ret.type = buffer_type_t::HOST;
      attr_ret.device = 0;
    } else {
      LCI_Assert(false, "Failed to get buffer attributes");
    }
  } else {
    switch (mem_type) {
      case CU_MEMORYTYPE_HOST:
        attr_ret.type = buffer_type_t::HOST;
        break;
      case CU_MEMORYTYPE_DEVICE:
        attr_ret.type = buffer_type_t::DEVICE;
        attr_ret.device = details::get_device_from_context(mem_ctx);
        break;
      case CU_MEMORYTYPE_ARRAY:
        LCI_Assert(false, "Array memory is not supported for now");
        break;
      case CU_MEMORYTYPE_UNIFIED:
        LCI_Assert(false, "Unified memory is not supported for now");
        break;
      default:
        // somehow it can be 0
        attr_ret.type = buffer_type_t::HOST;
    }
  }
  return attr_ret;
}

}  // namespace accelerator
}  // namespace lci