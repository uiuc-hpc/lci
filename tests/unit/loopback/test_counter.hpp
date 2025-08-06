// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

namespace test_counter
{
void my_counter_signal(lci::comp_t comp)
{
  lci::status_t status;
  status.set_done();
  lci::comp_signal(comp, status);
}

void my_counter_get(lci::comp_t comp, int64_t* p_out)
{
  *p_out = lci::counter_get(comp);
}

TEST(COUNTER, singlethread)
{
  const int n = 10;
  lci::g_runtime_init();
  auto comp = lci::alloc_counter_x()();

  for (int i = 0; i < n; i++) {
    my_counter_signal(comp);
    int64_t result;
    my_counter_get(comp, &result);
    ASSERT_EQ(result, i + 1);
  }

  lci::free_comp(&comp);
  lci::g_runtime_fina();
}

// all threads put and get
void test_multithread0(lci::comp_t comp)
{
  my_counter_signal(comp);
}

TEST(COUNTER, multithread0)
{
  lci::g_runtime_init();
  const int threshold = util::NTHREADS;
  lci::comp_t comp = lci::alloc_counter_x()();

  std::vector<std::thread> threads;
  for (int i = 0; i < threshold + 1; i++) {
    std::thread t(test_multithread0, comp);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }
  ASSERT_EQ(lci::counter_get(comp), threshold + 1);

  lci::free_comp(&comp);
  lci::g_runtime_fina();
}

}  // namespace test_counter