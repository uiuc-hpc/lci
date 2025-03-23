// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <random>
#include <iterator>
#include "lci.hpp"
#include "util.hpp"

TEST(COMM_BROADCAST, broadcast)
{
  lci::g_runtime_init();

  int rank = lci::get_rank();
  int nranks = lci::get_nranks();

  for (int root = 0; root < nranks; ++root) {
    uint64_t data = 0;
    if (rank == root) {
      data = 0xdeadbeef;
    }
    lci::broadcast(root, &data, sizeof(data));
    ASSERT_EQ(data, 0xdeadbeef);
  }
  lci::g_runtime_fina();
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}