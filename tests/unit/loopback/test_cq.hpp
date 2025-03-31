// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

namespace test_cq
{
void my_cq_push(lci::comp_t comp, uint64_t i)
{
  lci::status_t status;
  status.error = lci::errorcode_t::ok;
  status.user_context = reinterpret_cast<void*>(i + 1);
  lci::comp_signal(comp, status);
}

uint64_t my_cq_pop(lci::comp_t comp)
{
  lci::status_t status;
  do {
    status = lci::cq_pop(comp);
  } while (status.error.is_retry());
  return reinterpret_cast<uint64_t>(status.user_context) - 1;
}

TEST(CQ, singlethread)
{
  const int n = 100;
  lci::g_runtime_init();
  auto comp = lci::alloc_cq();
  for (uint64_t i = 0; i < n; i++) {
    my_cq_push(comp, i);
  }
  for (uint64_t i = 0; i < n; i++) {
    ASSERT_EQ(my_cq_pop(comp), i);
  }
  lci::free_comp(&comp);
  lci::g_runtime_fina();
}

// all threads put and get
void test_multithread0(lci::comp_t cq, int start, int n, bool flags[])
{
  for (uint64_t i = 0; i < n; i++) {
    my_cq_push(cq, start + i);
  }
  for (int i = 0; i < n; i++) {
    uint64_t idx = my_cq_pop(cq);
    ASSERT_EQ(flags[idx], false);
    flags[idx] = true;
  }
}

TEST(CQ, multithread0)
{
  lci::g_runtime_init();
  const int nthreads = 16;
  const int n = 10000;
  ASSERT_EQ(n % nthreads, 0);
  const int n_per_thread = n / nthreads;
  bool flags[n];
  memset(flags, 0, sizeof(flags));
  lci::comp_t comp = lci::alloc_cq_x().default_length(nthreads * n)();
  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_multithread0, std::ref(comp), i * n_per_thread,
                  n_per_thread, flags);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }
  for (int i = 0; i < n; i++) {
    ASSERT_EQ(flags[i], true);
  }
  lci::free_comp(&comp);
  lci::g_runtime_fina();
}

}  // namespace test_cq