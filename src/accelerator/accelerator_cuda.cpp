// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

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
std::vector<CUdevice> g_devices;

bool check_GPUDirectRDMA_support()
{
  // Check if the nv_peer_mem module is loaded
  const char* possible_paths[] = {
      "/sys/kernel/mm/memory_peers/nv_mem/version",
      "/sys/kernel/mm/memory_peers/nv_mem_nc/version",
      "/sys/module/nvidia_peermem/version",
  };
  bool is_kernel_module_loaded = false;
  for (size_t i = 0; i < sizeof(possible_paths) / sizeof(possible_paths[0]);
       i++) {
    if (access(possible_paths[i], F_OK) != -1) {
      is_kernel_module_loaded = true;
      break;
    }
  }
  LCI_Log(LOG_INFO, "cuda",
          "Checking GPUDirectRDMA: whether the kernel module is loaded: %d\n",
          is_kernel_module_loaded);
  // if (!is_kernel_module_loaded) return false;

  // Check if CUDA supports GPUDirectRDMA
  int is_cuda_support_gdr = 0;
#if CUDART_VERSION >= 11030
  int driverVersion;
  CUDACHECK(cudaDriverGetVersion(&driverVersion));
  if (driverVersion >= 11030) {
    int cudaDev;
    CUDACHECK(cudaGetDevice(&cudaDev));
    CUDACHECK(cudaDeviceGetAttribute(
        &is_cuda_support_gdr, cudaDevAttrGPUDirectRDMASupported, cudaDev));
  } else
#endif
  {
    LCI_Log(LOG_INFO, "cuda",
            "Checking GPUDirectRDMA: CUDA version is too old, assume it "
            "supports GPUDirectRDMA\n");
    is_cuda_support_gdr = 1;
  }
  LCI_Log(LOG_INFO, "cuda",
          "Checking GPUDirectRDMA: whether CUDA supports GPUDirectRDMA: %d\n",
          is_cuda_support_gdr);
  return is_kernel_module_loaded && is_cuda_support_gdr;
}

bool check_dmabuf_support()
{
  // 1. Check CUDA-level DMA-BUF support
  int is_cuda_support_dmabuf = 0;

#if CUDART_VERSION >= 11070
  int driverVersion;
  CUDACHECK(cudaDriverGetVersion(&driverVersion));
  if (driverVersion >= 11070) {
    CUdevice dev;
    CU_CHECK(cuDeviceGet(&dev, 0));

    CUresult res = cuDeviceGetAttribute(
        &is_cuda_support_dmabuf, CU_DEVICE_ATTRIBUTE_DMA_BUF_SUPPORTED, dev);
    if (res != CUDA_SUCCESS) {
      LCI_Log(LOG_WARN, "cuda",
              "cuDeviceGetAttribute(DMA_BUF_SUPPORTED) failed.\n");
      is_cuda_support_dmabuf = 0;
    }
  } else
#endif
  {
    LCI_Log(LOG_INFO, "cuda",
            "CUDA version < 11.7, DMA-BUF attribute unsupported.\n");
    is_cuda_support_dmabuf = 0;
  }

  LCI_Log(LOG_INFO, "cuda",
          "Checking DMA-BUF: whether CUDA supports DMA-BUF: %d\n",
          is_cuda_support_dmabuf);

  // if (!is_cuda_support_dmabuf) return false;

  // // 2. Check kernel/IB driver DMA-BUF support using dummy fd
  // struct ibv_device** dev_list = ibv_get_device_list(NULL);
  // if (!dev_list || !dev_list[0]) {
  //   LCI_Log(LOG_WARN, "ib", "No InfiniBand devices found.\n");
  //   return false;
  // }

  // struct ibv_context* context = ibv_open_device(dev_list[0]);
  // if (!context) {
  //   LCI_Log(LOG_WARN, "ib", "Failed to open IB device.\n");
  //   ibv_free_device_list(dev_list);
  //   return false;
  // }

  // struct ibv_pd* pd = ibv_alloc_pd(context);
  // if (!pd) {
  //   LCI_Log(LOG_WARN, "ib", "Failed to allocate PD.\n");
  //   ibv_close_device(context);
  //   ibv_free_device_list(dev_list);
  //   return false;
  // }

  // errno = 0;
  // void* mr = ibv_reg_dmabuf_mr(pd, 0, 0, 0, -1, 0);  // Dummy registration
  // int err = errno;
  // ibv_dealloc_pd(pd);
  // ibv_close_device(context);
  // ibv_free_device_list(dev_list);

  // bool is_kernel_support_dmabuf = (mr == NULL && (err == EBADF));
  // LCI_Log(LOG_INFO, "ib", "Checking DMA-BUF: ibv_reg_dmabuf_mr errno = %d,
  // support = %d\n", err, is_kernel_support_dmabuf);

  // return is_kernel_support_dmabuf && is_cuda_support_dmabuf;
  return is_cuda_support_dmabuf;
}

int get_device_from_context(CUcontext ctx)
{
  int ret = -1;
  CUdevice dev;
  CU_CHECK(cuCtxPushCurrent(ctx));
  CU_CHECK(cuCtxGetDevice(&dev));
  for (int i = 0; i < static_cast<int>(g_devices.size()); i++) {
    if (g_devices[i] == dev) {
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
    details::g_devices.push_back(dev);
  }
  // Purely for logging information
  details::check_GPUDirectRDMA_support();
  details::check_dmabuf_support();
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
          "get_buffer_attr(%p) -> mem_type: %d, mem_ctx: %p, is_managed: %u\n",
          ptr, mem_type, mem_ctx, is_managed);
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