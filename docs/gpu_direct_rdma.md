@page gpu_direct_rdma GPU Direct RDMA

[TOC]

# Overview

LCI can send/receive/put/get GPU-resident buffers directly over RDMA, as long as
the backend and driver stack support GPU Direct RDMA.

This document summarizes the required steps and caveats.

# Requirements

## 1) Enable CUDA support at build time

Build LCI with CUDA support enabled:

```bash
cmake -DLCI_USE_CUDA=ON ...
```

Other CUDA-related options you may want to consider:
```bash
-DLCI_CUDA_ARCH=<your_cuda_architecture>  # e.g., 80 for A100, 90 for H100
-DLCI_CUDA_STANDARD=<cuda_standard>      # e.g., 20
```

## 2) Ensure memory registration is GPU-aware

LCI needs to know that a buffer is device memory. You can satisfy this in two
ways:

- Explicitly register the buffer using `lci::register_memory`. LCI will detect
  that the buffer is GPU memory and register it accordingly.
- Use the OFF optional argument `mr` and set it to `MR_DEVICE` or `MR_UNKNOWN`
  when posting operations. `MR_UNKNOWN` lets LCI detect the memory type at runtime.

## 3) Pass GPU buffers to communication APIs

Make sure the buffer pointer passed to send/recv/put/get APIs refers to GPU memory
(e.g., a CUDA device pointer), not a host buffer.

# Example

Refer to `tests/unit/accelerator` for examples of using LCI with GPU Direct RDMA.

# Caveats

- It is challenging to make Active messages GPU-direct because receive buffers are
  allocated by LCI on the host. If you need GPU-resident AM receives, overload the
  packet pool with a GPU-aware allocator.
