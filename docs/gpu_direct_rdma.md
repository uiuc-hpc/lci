@page gpu_direct_rdma GPU Direct RDMA

[TOC]

# Overview

LCI can send/receive/put/get GPU-resident buffers directly over RDMA, as long as
the hardware and driver stack support GPU Direct RDMA.

LCI supports both NVIDIA GPUs (via CUDA) and AMD GPUs (via HIP/ROCm).

This document summarizes the required steps and notes.

# Requirements

## 1) Enable GPU support at build time

Build LCI with CUDA and/or HIP support enabled:

**For NVIDIA GPUs (CUDA):**
```bash
cmake -DLCI_USE_CUDA=ON ...
```

Other CUDA-related options you may want to consider:
```bash
-DLCI_CUDA_ARCH=<your_cuda_architecture>  # e.g., 80 for A100, 90 for H100
-DLCI_CUDA_STANDARD=<cuda_standard>      # e.g., 20
```

**For AMD GPUs (HIP/ROCm):**
```bash
cmake -DLCI_USE_HIP=ON ...
```

Other HIP-related options you may want to consider:
```bash
-DLCI_HIP_ARCH=<your_hip_architecture>   # e.g., gfx90a for MI250X, gfx942 for MI300A/MI300X/MI325X, gfx950 for MI350X/MI355X
-DLCI_HIP_STANDARD=<hip_standard>        # e.g., 20
```

**Note:** `LCI_USE_CUDA` and `LCI_USE_HIP` are mutually exclusive. Choose the GPU vendor matching your hardware at compile time. For heterogeneous clusters with different GPU vendors on different nodes, compile separate binaries for each node type.

## 2) Ensure memory registration is GPU-aware

LCI needs to know that a buffer is device memory. You can satisfy this in two
ways:

- Explicitly register the buffer using `lci::register_memory`. LCI will detect
  that the buffer is GPU memory and register it accordingly.
- Use the optional argument `mr` and set it to `MR_DEVICE` or `MR_UNKNOWN`
  when posting operations. `MR_UNKNOWN` lets LCI detect the memory type at runtime.

## 3) Pass GPU buffers to communication APIs

Just pass GPU memory pointers to send/recv/put/get APIs as usual.

# Example

Sending a GPU buffer with automatic memory registration:
```cpp
// If you know the buffer is GPU memory:
lci::status_t status = lci::post_send_x(rank, gpu_buffer, msg_size, tag, comp).mr(lci::MR_DEVICE)();
// If you are unsure about the memory type, use MR_UNKNOWN:
lci::status_t status = lci::post_send_x(rank, generic_buffer, msg_size, tag, comp).mr(lci::MR_UNKNOWN)();
```

Alternatively, You can explicitly register a GPU buffer and using it in a send operation:
```cpp
lci::mr_t mr = lci::register_memory(gpu_buffer, size);
lci::status_t status = lci::post_send_x(rank, gpu_buffer, msg_size, tag, comp).mr(mr)();
// You can also use part of the registered region for communication
lci::status_t status = lci::post_send_x(rank, gpu_buffer + offset, smaller_size, tag, comp).mr(mr)();
...
lci::deregister_memory(mr);
```

Same for put/get operations:
```cpp
// Register the GPU buffer and exchange the rmr_t with all processes
lci::mr_t mr = lci::register_memory(gpu_buffer, size);
lci::rmr_t rmr = lci::get_rmr(mr);
std::vector<lci::rmr_t> rmrs;
lci::allgather(&rmr, rmrs.data(), sizeof(lci::rmr_t));
// Perform put/get operation
lci::status_t status = lci::post_put_x(rank, local_gpu_buffer, size, comp, remote_offset, rmrs[rank]).mr(mr)();
...
lci::deregister_memory(mr);
```

Refer to `tests/unit/accelerator` for a complete example of using LCI with GPU Direct RDMA.

# Notes

- LCI supports both NVIDIA GPUs (CUDA) and AMD GPUs (HIP/ROCm), but they are mutually exclusive at build time.
- It is challenging to make Active messages GPU-direct because receive buffers are
  allocated by LCI on the host. If you need GPU-resident AM receives, overload the
  packet pool with a GPU-aware allocator.
