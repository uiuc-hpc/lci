// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

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
  const int msg_size = 65536;

  lci::g_runtime_init();
  int rank_me = lci::get_rank_me();
  int rank_n = lci::get_rank_n();
  
  // allocate buffers
  float* src_buffer = nullptr;
  CUDA_CHECK(cudaMalloc(&src_buffer, msg_size));
  CUDA_CHECK(cudaMemset(src_buffer, 0, msg_size));

  float* dst_buffer = nullptr;
  CUDA_CHECK(cudaMalloc(&dst_buffer, msg_size));
  CUDA_CHECK(cudaMemset(dst_buffer, 0, msg_size));

  // initialize source buffer
  init_buffer<<<1, msg_size>>>(src_buffer, rank_me, msg_size);
  CUDA_CHECK(cudaDeviceSynchronize());

  // ring, should also work for rank_n == 1
  int left = (rank_me - 1 + rank_n) % rank_n;
  int right = (rank_me + 1) % rank_n;
  lci::comp_t sync = lci::alloc_sync_x().threshold(2)();
  lci::post_send_x(left, src_buffer, msg_size, 0, sync).allow_retry(false)();
  lci::post_recv_x(right, dst_buffer, msg_size, 0, sync).allow_retry(false)();
  lci::sync_wait(sync, nullptr);
  verify_buffer(dst_buffer, right, msg_size);
  
  CUDA_CHECK(cudaFree(src_buffer));
  CUDA_CHECK(cudaFree(dst_buffer));

  lci::g_runtime_fina();
  return 0;
}