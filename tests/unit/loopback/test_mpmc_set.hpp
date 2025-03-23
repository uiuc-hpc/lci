// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"
#include <thread>

namespace test_packet_pool
{
TEST(MPMC_SET, singlethread)
{
  lci::global_initialize();
  lci::mpmc_set_t pool(0, 1);
  const int n = 1000;
  bool flags[n];
  memset(flags, 0, sizeof(flags));
  for (int i = 0; i < n; i++) {
    pool.put(reinterpret_cast<void*>(i + 1));
  }
  for (int i = 0; i < n; i++) {
    void* val = pool.get();
    uint64_t idx = reinterpret_cast<uint64_t>(val) - 1;
    ASSERT_EQ(flags[idx], false);
    flags[idx] = true;
  }
  for (int i = 0; i < n; i++) {
    ASSERT_EQ(flags[i], true);
  }
  lci::global_finalize();
}

// all threads put and get
void test_multithread0(lci::mpmc_set_t& pool, int start, int n, bool flags[])
{
  for (uint64_t i = 0; i < n; i++) {
    pool.put(reinterpret_cast<void*>(start + i + 1));
  }
  int ncomps = 0;
  while (ncomps < n) {
    void* val = pool.get();
    if (val) {
      uint64_t idx = reinterpret_cast<uint64_t>(val) - 1;
      ASSERT_EQ(flags[idx], false);
      flags[idx] = true;
      ++ncomps;
    }
  }
}

TEST(MPMC_SET, multithread0)
{
  lci::global_initialize();
  const int nthreads = 16;
  const int n = 100000;
  ASSERT_EQ(n % nthreads, 0);
  const int n_per_thread = n / nthreads;
  bool flags[n];
  memset(flags, 0, sizeof(flags));
  lci::mpmc_set_t pool(0, 1);
  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_multithread0, std::ref(pool), i * n_per_thread,
                  n_per_thread, flags);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }
  for (int i = 0; i < n; i++) {
    ASSERT_EQ(flags[i], true);
  }
  lci::global_finalize();
}

// 1 thread put and all threads get (testing work stealing)
void test_multithread1(lci::mpmc_set_t& pool, int start, int n, bool flags[])
{
  int ncomps = 0;
  while (ncomps < n) {
    void* val = pool.get();
    if (val) {
      uint64_t idx = reinterpret_cast<uint64_t>(val) - 1;
      ASSERT_EQ(flags[idx], false);
      flags[idx] = true;
      ++ncomps;
    }
  }
}

TEST(MPMC_SET, multithread1)
{
  lci::global_initialize();
  const int nthreads = 16;
  const int n = 100000;
  ASSERT_EQ(n % nthreads, 0);
  const int n_per_thread = n / nthreads;
  bool flags[n];
  memset(flags, 0, sizeof(flags));
  lci::mpmc_set_t pool(0, 1);
  for (int i = 0; i < n; i++) {
    pool.put(reinterpret_cast<void*>(i + 1));
  }
  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_multithread1, std::ref(pool), i * n_per_thread,
                  n_per_thread, flags);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }
  for (int i = 0; i < n; i++) {
    ASSERT_EQ(flags[i], true);
  }
  lci::global_finalize();
}

// all threads put and all threads get
void test_multithread2(lci::mpmc_set_t& pool, int start, int n, bool flags[])
{
  for (int i = 0; i < n; i++) {
    pool.put(reinterpret_cast<void*>(start + i + 1));
  }
}

TEST(MPMC_SET, multithread2)
{
  lci::global_initialize();
  const int nthreads = 16;
  const int n = 100000;
  ASSERT_EQ(n % nthreads, 0);
  const int n_per_thread = n / nthreads;
  bool flags[n];
  memset(flags, 0, sizeof(flags));
  lci::mpmc_set_t pool(0, 1);
  // all other threads will and put to pool
  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_multithread2, std::ref(pool), i * n_per_thread,
                  n_per_thread, flags);
    threads.push_back(std::move(t));
  }
  // the main thread will get from pool2
  int ncomps = 0;
  while (ncomps < n) {
    void* val = pool.get();
    if (val) {
      uint64_t idx = reinterpret_cast<uint64_t>(val) - 1;
      ASSERT_EQ(flags[idx], false);
      flags[idx] = true;
      ++ncomps;
    }
  }
  for (auto& t : threads) {
    t.join();
  }
  for (int i = 0; i < n; i++) {
    ASSERT_EQ(flags[i], true);
  }
  lci::global_finalize();
}

// all threads put and all threads get
void test_multithread3(lci::mpmc_set_t& pool0, lci::mpmc_set_t& pool1,
                       lci::mpmc_set_t& pool2, int start, int n, bool flags[])
{
  int ncomps = 0;
  while (ncomps < n) {
    // try poll pool1 10 times
    for (int j = 0; j < 10; ++j) {
      void* val1 = pool1.get();
      if (val1) {
        pool2.put(val1);
        if (++ncomps == n) {
          break;
        }
      }
    }
    // and then poll pool0 once
    void* val0 = pool0.get();
    if (val0) {
      pool1.put(val0);
    }
  }
}

TEST(MPMC_SET, multithread3)
{
  lci::global_initialize();
  const int nthreads = 16;
  const int n = 100000;
  ASSERT_EQ(n % nthreads, 0);
  const int n_per_thread = n / nthreads;
  bool flags[n];
  memset(flags, 0, sizeof(flags));
  lci::mpmc_set_t pool0(0, 1);
  lci::mpmc_set_t pool1(0, 1);
  lci::mpmc_set_t pool2(0, 1);
  // the main thread will put to pool0
  for (int i = 0; i < n; i++) {
    pool0.put(reinterpret_cast<void*>(i + 1));
  }
  // all other threads will get from pool0, put to/get from pool1, and put to
  // pool2
  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_multithread3, std::ref(pool0), std::ref(pool1),
                  std::ref(pool2), i * n_per_thread, n_per_thread, flags);
    threads.push_back(std::move(t));
  }
  // the main thread will get from pool2
  int ncomps = 0;
  while (ncomps < n) {
    void* val = pool2.get();
    if (val) {
      uint64_t idx = reinterpret_cast<uint64_t>(val) - 1;
      ASSERT_EQ(flags[idx], false);
      flags[idx] = true;
      ++ncomps;
    }
  }
  for (auto& t : threads) {
    t.join();
  }
  for (int i = 0; i < n; i++) {
    ASSERT_EQ(flags[i], true);
  }
  lci::global_finalize();
}
}  // namespace test_packet_pool
