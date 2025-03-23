// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

namespace test_sync
{
void my_sync_signal(lci::comp_t comp, uint64_t i)
{
  lci::status_t status;
  status.error = lci::errorcode_t::ok;
  status.user_context = reinterpret_cast<void*>(i + 1);
  lci::comp_signal(comp, status);
}

void my_sync_wait(lci::comp_t comp, int count, uint64_t* p_out)
{
  std::vector<lci::status_t> statuses(count);
  lci::sync_wait(comp, statuses.data());
  for (int i = 0; i < count; i++) {
    p_out[i] = reinterpret_cast<uint64_t>(statuses[i].user_context) - 1;
  }
}

TEST(SYNC, singlethread)
{
  const int n = 10;
  const int threshold = 3;
  lci::g_runtime_init();
  auto comp = lci::alloc_sync_x().threshold(threshold)();
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < threshold; j++) {
      my_sync_signal(comp, i + j);
    }
    uint64_t results[threshold];
    my_sync_wait(comp, threshold, results);
    for (int j = 0; j < threshold; j++) {
      ASSERT_EQ(results[j], i + j);
    }
  }
  lci::free_comp(&comp);
  lci::g_runtime_fina();
}

// all threads put and get
void test_multithread0(int id, lci::comp_t comp, int threshold, int n,
                       std::atomic<int>& barrier)
{
  for (int i = 0; i < n; i++) {
    if (id < threshold) {
      // producer
      while (barrier.load() < i) continue;
      my_sync_signal(comp, i + id);
    } else {
      // consumer
      uint64_t results[threshold];
      my_sync_wait(comp, threshold, results);
      bool flags[threshold];
      memset(flags, false, sizeof(flags));
      for (int j = 0; j < threshold; j++) {
        uint64_t idx = results[j] - i;
        ASSERT_EQ(flags[idx], false);
        flags[idx] = true;
      }
      ++barrier;
    }
  }
}

TEST(SYNC, multithread0)
{
  lci::g_runtime_init();
  const int threshold = 8;
  const int n = 1000;

  lci::comp_t comp = lci::alloc_sync_x().threshold(threshold)();
  std::atomic<int> barrier(0);

  std::vector<std::thread> threads;
  for (int i = 0; i < threshold + 1; i++) {
    std::thread t(test_multithread0, i, comp, threshold, n, std::ref(barrier));
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }
  lci::free_comp(&comp);
  lci::g_runtime_fina();
}

}  // namespace test_sync