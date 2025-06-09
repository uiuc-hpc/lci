// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include <iostream>
#include <unistd.h>
#include <cassert>

#include "lci.hpp"
#include "cuda_util.hpp"

__global__ void init_buffer(float* d_buf, float value, int N) {
    int idx = threadIdx.x + blockIdx.x * blockDim.x;
    if (idx < N) {
        d_buf[idx] = value;
    }
}

__global__ void verify_buffer(float* d_buf, float expected, int N, int* d_flag) {
  int idx = threadIdx.x + blockIdx.x * blockDim.x;
  if (idx < N) {
    if (d_buf[idx] != expected) {
      *d_flag = 1;
    }
  }
}

void verify_buffer(float* d_buf, float expected, int N) {
  int* d_flag = nullptr;
  CUDA_CHECK(cudaMalloc(&d_flag, sizeof(int)));
  CUDA_CHECK(cudaMemset(d_flag, 0, sizeof(int)));
  verify_buffer<<<1, N>>>(d_buf, expected, N, d_flag);
  int h_flag = 0;
  CUDA_CHECK(cudaMemcpy(&h_flag, d_flag, sizeof(int), cudaMemcpyDeviceToHost));
  CUDA_CHECK(cudaFree(d_flag));
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
  CUDA_CHECK(cudaMalloc(&src_buffer, msg_size));
  CUDA_CHECK(cudaMemset(src_buffer, -1, msg_size));

  float* dst_buffer = nullptr;
  CUDA_CHECK(cudaMalloc(&dst_buffer, msg_size));
  CUDA_CHECK(cudaMemset(dst_buffer, -1, msg_size));

  // initialize source buffer
  init_buffer<<<1, N>>>(src_buffer, rank_me, N);
  CUDA_CHECK(cudaDeviceSynchronize());

  // ring, should also work for rank_n == 1
  int left = (rank_me - 1 + rank_n) % rank_n;
  int right = (rank_me + 1) % rank_n;
  lci::comp_t sync = lci::alloc_sync_x().threshold(2)();
  lci::post_send_x(left, src_buffer, msg_size, 0, sync).allow_retry(false).mr(lci::MR_DEVICE)();
  lci::post_recv_x(right, dst_buffer, msg_size, 0, sync).allow_retry(false).mr(lci::MR_DEVICE)();
  lci::sync_wait(sync, nullptr);
  verify_buffer(dst_buffer, right, N);

  if (rank_me == 0) {
    std::cout << "Test passed" << std::endl;
    std::cout << "message size: " << msg_size << std::endl;
    std::cout << "rank_n: " << rank_n << std::endl;
  }
  
  CUDA_CHECK(cudaFree(src_buffer));
  CUDA_CHECK(cudaFree(dst_buffer));

  lci::g_runtime_fina();
  return 0;
}