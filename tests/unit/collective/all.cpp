// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <random>
#include <iterator>
#include "lci.hpp"
#include "util.hpp"

TEST(COMM_COLL, broadcast)
{
  lci::g_runtime_init();

  int rank = lci::get_rank();
  int nranks = lci::get_nranks();

  for (int root = 0; root < nranks; ++root) {
    uint64_t data = 0;
    if (rank == root) {
      data = 0xdeadbeef;
    }
    lci::broadcast(&data, sizeof(data), root);
    ASSERT_EQ(data, 0xdeadbeef);
  }
  lci::g_runtime_fina();
}

void reduce_op(const void* left, const void* right, void* dst, size_t n)
{
  const uint64_t* left_ = static_cast<const uint64_t*>(left);
  const uint64_t* right_ = static_cast<const uint64_t*>(right);
  uint64_t* dst_ = static_cast<uint64_t*>(dst);
  for (size_t i = 0; i < n; ++i) {
    dst_[i] = left_[i] + right_[i];
  }
}

TEST(COMM_COLL, reduce_in_place)
{
  lci::g_runtime_init();

  int rank = lci::get_rank();
  int nranks = lci::get_nranks();

  for (int root = 0; root < nranks; ++root) {
    uint64_t data = rank;
    lci::reduce(&data, &data, 1, sizeof(data), reduce_op, root);
    if (rank == root) {
      ASSERT_EQ(data, (nranks - 1) * nranks / 2);
    } else {
      ASSERT_EQ(data, rank);
    }
  }
  lci::g_runtime_fina();
}

TEST(COMM_COLL, reduce)
{
  lci::g_runtime_init();

  int rank = lci::get_rank();
  int nranks = lci::get_nranks();

  for (int root = 0; root < nranks; ++root) {
    uint64_t data = rank;
    uint64_t result = -1;
    lci::reduce(&data, &result, 1, sizeof(data), reduce_op, root);
    if (rank == root) ASSERT_EQ(result, (nranks - 1) * nranks / 2);
    ASSERT_EQ(data, rank);
  }
  lci::g_runtime_fina();
}

TEST(COMM_COLL, alltoall)
{
  lci::g_runtime_init();

  int rank = lci::get_rank();
  int nranks = lci::get_nranks();

  std::vector<uint64_t> sendbuf(nranks, rank);
  std::vector<uint64_t> recvbuf(nranks, -1);

  lci::alltoall(sendbuf.data(), recvbuf.data(), sizeof(uint64_t));

  for (int i = 0; i < nranks; ++i) {
    ASSERT_EQ(recvbuf[i], i);
  }

  lci::g_runtime_fina();
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}