// Copyright (c) 2026 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "hip_util.hpp"
#include "lci.hpp"

namespace
{
constexpr int kDefaultMsgSize = 65536;
constexpr int kDefaultIterations = 10000;
}  // namespace

int main(int argc, char** argv)
{
  int msg_size = kDefaultMsgSize;
  int niters = kDefaultIterations;
  if (argc >= 2) {
    msg_size = std::atoi(argv[1]);
  }
  if (argc >= 3) {
    niters = std::atoi(argv[2]);
  }

  assert(msg_size > 0);
  assert(niters > 0);

  lci::g_runtime_init();
  const int rank_me = lci::get_rank_me();
  const int rank_n = lci::get_rank_n();

  void* buffer = nullptr;
  HIP_CHECK(hipMalloc(&buffer, msg_size));
  HIP_CHECK(hipMemset(buffer, rank_me, msg_size));
  HIP_CHECK(hipDeviceSynchronize());

  std::vector<lci::mr_t> mrs(niters);
  for (int i = 0; i < niters; ++i) {
    mrs[i] = lci::register_memory(buffer, msg_size);
  }
  for (int i = niters - 1; i >= 0; --i) {
    lci::deregister_memory(&mrs[i]);
  }

  HIP_CHECK(hipFree(buffer));
  lci::g_runtime_fina();

  if (rank_me == 0) {
    std::cout << "GPU memory registration stress test passed" << std::endl;
    std::cout << "message size: " << msg_size << std::endl;
    std::cout << "iterations: " << niters << std::endl;
    std::cout << "rank_n: " << rank_n << std::endl;
  }
  return 0;
}
