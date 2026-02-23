// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include <hip/hip_runtime.h>
#include <hsa/hsa.h>
#include <hsa/hsa_ext_amd.h>
#include "lci_internal.hpp"

#define HIP_CHECK(call)                                                \
  do {                                                                 \
    hipError_t err = call;                                             \
    if (hipSuccess != err) {                                           \
      fprintf(stderr, "HIP failure %s:%d: '%s'\n", __FILE__, __LINE__, \
              hipGetErrorString(err));                                 \
      exit(EXIT_FAILURE);                                              \
    }                                                                  \
  } while (0)

#define HIP_DRIVER_CHECK(call)                                                 \
  do {                                                                         \
    hipError_t result = call;                                                  \
    if (hipSuccess != result) {                                                \
      fprintf(stderr, "HIP failure %s:%d: '%s'\n", __FILE__, __LINE__, #call); \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)

#define HSA_CHECK(call)                                                \
  do {                                                                 \
    hsa_status_t status = call;                                        \
    if (HSA_STATUS_SUCCESS != status) {                                \
      const char* msg = nullptr;                                       \
      hsa_status_string(status, &msg);                                 \
      fprintf(stderr, "HSA failure %s:%d: '%s'\n", __FILE__, __LINE__, \
              msg ? msg : "unknown");                                  \
      exit(EXIT_FAILURE);                                              \
    }                                                                  \
  } while (0)

namespace lci
{
namespace accelerator
{
namespace details
{
std::vector<hipDevice_t> g_devices;

bool check_GPUDirectRDMA_support()
{
  // Check if the AMD peer memory module is loaded
  const char* possible_paths[] = {
      "/sys/kernel/mm/memory_peers/amdkfd/version",
      "/sys/kernel/memory_peers/amdkfd/version",
      "/sys/memory_peers/amdkfd/version",
  };
  bool is_kernel_module_loaded = false;
  for (size_t i = 0; i < sizeof(possible_paths) / sizeof(possible_paths[0]);
       i++) {
    if (access(possible_paths[i], F_OK) != -1) {
      is_kernel_module_loaded = true;
      break;
    }
  }

  // Fallback: check /proc/kallsyms for ib_register_peer_memory_client
  // (following rccl's approach)
  if (!is_kernel_module_loaded) {
    FILE* fp = fopen("/proc/kallsyms", "r");
    if (fp) {
      char buf[256];
      while (fgets(buf, sizeof(buf), fp) != NULL) {
        if (strstr(buf, "t ib_register_peer_memory_client") != NULL ||
            strstr(buf, "T ib_register_peer_memory_client") != NULL) {
          is_kernel_module_loaded = true;
          break;
        }
      }
      fclose(fp);
    }
  }

  LCI_Log(LOG_INFO, "hip",
          "Checking GPUDirectRDMA: whether the kernel module is loaded: %d\n",
          is_kernel_module_loaded);

  // Check if HIP supports GPUDirectRDMA
  // HIP doesn't have a direct equivalent to cudaDevAttrGPUDirectRDMASupported.
  // Assume GPUDirect RDMA is supported if kernel modules are present.
  int is_hip_support_gdr = is_kernel_module_loaded ? 1 : 0;

  LCI_Log(LOG_INFO, "hip",
          "Checking GPUDirectRDMA: whether HIP supports GPUDirectRDMA: %d\n",
          is_hip_support_gdr);
  return is_kernel_module_loaded && is_hip_support_gdr;
}

bool check_dmabuf_support()
{
  // Assume DMA-BUF is always available on modern ROCm systems.
  LCI_Log(LOG_INFO, "hip", "Assuming DMA-BUF support is available.\n");
  return true;
}

}  // namespace details

void initialize()
{
  HIP_DRIVER_CHECK(hipInit(0));
  int num_devices;
  HIP_CHECK(hipGetDeviceCount(&num_devices));
  for (int i = 0; i < num_devices; i++) {
    hipDevice_t dev;
    HIP_DRIVER_CHECK(hipDeviceGet(&dev, i));
    char name[256];
    HIP_DRIVER_CHECK(hipDeviceGetName(name, 256, dev));
    LCI_Log(LOG_INFO, "hip", "Found HIP device %d: %s\n", dev, name);
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

  hipPointerAttribute_t attributes;
  hipError_t result = hipPointerGetAttributes(&attributes, ptr);

#if HIP_VERSION >= 50731921
  hipMemoryType mem_type = attributes.type;
#else
  hipMemoryType mem_type = attributes.memoryType;
#endif

  LCI_Log(LOG_TRACE, "hip", "get_buffer_attr(%p) -> mem_type: %d\n", ptr,
          mem_type);

  if (hipSuccess != result) {
    if (hipErrorInvalidValue == result) {
      // Unregistered host memory
      attr_ret.type = buffer_type_t::HOST;
      attr_ret.device = 0;
    } else {
      LCI_Assert(false, "Failed to get buffer attributes");
    }
  } else {
    switch (mem_type) {
      case hipMemoryTypeHost:
        attr_ret.type = buffer_type_t::HOST;
        break;
      case hipMemoryTypeDevice:
        attr_ret.type = buffer_type_t::DEVICE;
        attr_ret.device = attributes.device;
        break;
      case hipMemoryTypeArray:
        LCI_Assert(false, "Array memory is not supported for now");
        break;
      case hipMemoryTypeUnified:
        LCI_Assert(false, "Unified memory is not supported for now");
        break;
      default:
        // Default to host memory
        attr_ret.type = buffer_type_t::HOST;
    }
  }
  return attr_ret;
}

int get_dmabuf_fd(const void* ptr, size_t size, uint64_t* offset)
{
  int dmabuf_fd = -1;
  uint64_t local_offset = 0;

  hsa_status_t status =
      hsa_amd_portable_export_dmabuf(ptr, size, &dmabuf_fd, &local_offset);
  if (status != HSA_STATUS_SUCCESS) {
    const char* msg = nullptr;
    hsa_status_string(status, &msg);
    LCI_Log(LOG_WARN, "hip", "hsa_amd_portable_export_dmabuf failed: %s\n",
            msg ? msg : "unknown");
    return -1;
  }

  if (offset) {
    *offset = local_offset;
  }
  return dmabuf_fd;
}

}  // namespace accelerator
}  // namespace lci
