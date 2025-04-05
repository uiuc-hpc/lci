// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"
#include <thread>

namespace test_mpmc_array
{
TEST(MPMC_ARRAY, singlethread)
{
  lci::global_initialize();
  const int total = 1000;
  int counter = 0;
  lci::mpmc_array_t<void*> array(1);
  for (int i = 0; i < total; i++) {
    array.put(i, reinterpret_cast<void*>(i + 1));
  }
  for (int i = 0; i < total; i++) {
    void* p = array.get(i);
    uint64_t val = reinterpret_cast<uint64_t>(array.get(i));
    ASSERT_EQ(val, i + 1);
  }
  lci::global_finalize();
}

// all threads put sequentially
void test_multithread0(lci::mpmc_array_t<void*>& array,
                       std::atomic<int>& counter, int total)
{
  int i = 0;
  while (true) {
    i = counter++;
    if (i >= total) {
      break;
    }
    array.put(i, reinterpret_cast<void*>(i + 1));
  }
}

TEST(MPMC_ARRAY, multithread0)
{
  lci::global_initialize();
  const int nthreads = 16;
  const int total = 1000;
  std::atomic<int> counter(0);
  lci::mpmc_array_t<void*> array(1);
  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_mpmc_array::test_multithread0, std::ref(array),
                  std::ref(counter), total);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }
  for (int i = 0; i < total; i++) {
    void* p = array.get(i);
    uint64_t val = reinterpret_cast<uint64_t>(array.get(i));
    ASSERT_EQ(val, i + 1);
  }
  lci::global_finalize();
}
}  // namespace test_mpmc_array
