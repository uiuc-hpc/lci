#ifndef CUDA_UTILS_H_
#define CUDA_UTILS_H_

#include <cuda_runtime_api.h>
#include "lci.h"
#include "macro.h"


typedef struct {
  int valid = 0;
  int ready = 0;
  LCI_error_t status;
  void* src;
  size_t size;
  int rank;
  int tag;
  LCI_endpoint_t ep;
} LCI_device_queue_entry_t;

typedef struct {
  size_t size;
  LCI_device_queue_entry_t* queue;
} LCI_device_queue_t;

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


__device__
LCI_error_t LCI_sendbc_device(void* device_queue, void* src, size_t size, int rank, int tag, LCI_endpoint_t ep)
{
  if (((LCI_device_queue_t*)device_queue)->size == 0) return LCI_ERR_FATAL; 
  LCI_device_queue_entry_t* device_queue_ptr = ((LCI_device_queue_t*)device_queue)->queue;
  
  int entry_idx = 0 // TODO: find empty entry, remove hardcoding

  device_queue_ptr[entry_idx].valid = 0;
  device_queue_ptr[entry_idx].ready = 0;
  device_queue_ptr[entry_idx].status = LCI_ERR_RETRY;
  device_queue_ptr[entry_idx].src   = src;
  device_queue_ptr[entry_idx].size  = size;
  device_queue_ptr[entry_idx].rank  = rank;
  device_queue_ptr[entry_idx].tag   = tag;
  device_queue_ptr[entry_idx].ep    = ep;
  threadfence();
  device_queue_ptr[entry_idx].valid = 1;

  return LCI_OK;
}


/*
  polls GPU queue, must be launched as a thread
*/
void device_send_helper(void* device_queue);
{
  LCI_device_queue_t device_queue_host_copy;
  while(1)
  {
    cudaMemcpy(&device_queue_host_copy, device_queue, sizeof(device_queue_host_copy), cudaMemcpyDeviceToHost);
    if (device_queue_host_copy.size == 0) return; // empty device queue, exit gracefully

    int entry_idx = 0 // TODO: find valid entry, remove hardcoding
    LCI_device_queue_entry_t* request = &(device_queue_host_copy.queue[0]);
    if (request->valid == 0) continue; // no vfalid request, keep waiting;

    // work on request, simply call public API
    request->status = LCI_sendbc(request->src, request->size, request->rank, request->tag, request->ep);

    // TODO what to do with the request return code? another stream to copy status back to GPU
  }
}


#endif // CUDA_UTILS_H_
