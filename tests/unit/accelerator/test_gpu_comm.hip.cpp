// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include <iostream>
#include <unistd.h>
#include <cassert>

#include "lci.hpp"
#include "hip_util.hpp"

__global__ void init_buffer(float* d_buf, float value, int N)
{
  int idx = threadIdx.x + blockIdx.x * blockDim.x;
  if (idx < N) {
    d_buf[idx] = value;
  }
}

__global__ void verify_buffer(float* d_buf, float expected, int N, int* d_flag)
{
  int idx = threadIdx.x + blockIdx.x * blockDim.x;
  if (idx < N) {
    if (d_buf[idx] != expected) {
      *d_flag = 1;
    }
  }
}

void verify_buffer(float* d_buf, float expected, int N)
{
  int* d_flag = nullptr;
  HIP_CHECK(hipMalloc(&d_flag, sizeof(int)));
  HIP_CHECK(hipMemset(d_flag, 0, sizeof(int)));
  hipLaunchKernelGGL(verify_buffer, dim3(1), dim3(N), 0, 0, d_buf, expected, N,
                     d_flag);
  int h_flag = 0;
  HIP_CHECK(hipMemcpy(&h_flag, d_flag, sizeof(int), hipMemcpyDeviceToHost));
  HIP_CHECK(hipFree(d_flag));
  if (h_flag == 1) {
    printf("Error: buffer verification failed\n");
    exit(1);
  }
}

int main(int argc, char** argv)
{
  int msg_size = 65536;
  if (argc >= 2) {
    msg_size = atoi(argv[1]);
  }

  assert(msg_size % sizeof(float) == 0);
  int N = msg_size / sizeof(float);

  lci::g_runtime_init();
  int rank_me = lci::get_rank_me();
  int rank_n = lci::get_rank_n();

  // allocate buffers
  float* src_buffer = nullptr;
  HIP_CHECK(hipMalloc(&src_buffer, msg_size));
  HIP_CHECK(hipMemset(src_buffer, -1, msg_size));

  float* dst_buffer = nullptr;
  HIP_CHECK(hipMalloc(&dst_buffer, msg_size));
  HIP_CHECK(hipMemset(dst_buffer, -1, msg_size));

  // initialize source buffer
  hipLaunchKernelGGL(init_buffer, dim3(1), dim3(N), 0, 0, src_buffer, rank_me,
                     N);
  HIP_CHECK(hipDeviceSynchronize());

  // ring, should also work for rank_n == 1
  int left = (rank_me - 1 + rank_n) % rank_n;
  int right = (rank_me + 1) % rank_n;
  lci::comp_t sync = lci::alloc_sync_x().threshold(2)();
  lci::post_send_x(left, src_buffer, msg_size, 0, sync)
      .allow_retry(false)
      .mr(lci::MR_DEVICE)();
  lci::post_recv_x(right, dst_buffer, msg_size, 0, sync)
      .allow_retry(false)
      .mr(lci::MR_DEVICE)();
  lci::sync_wait(sync, nullptr);
  verify_buffer(dst_buffer, right, N);

  if (rank_me == 0) {
    std::cout << "Test passed" << std::endl;
    std::cout << "message size: " << msg_size << std::endl;
    std::cout << "rank_n: " << rank_n << std::endl;
  }

  HIP_CHECK(hipFree(src_buffer));
  HIP_CHECK(hipFree(dst_buffer));

  lci::g_runtime_fina();
  return 0;
}
