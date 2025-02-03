namespace test_cq
{
void my_cq_push(lci::comp_t comp, uint64_t i)
{
  lci::status_t status;
  status.ctx = reinterpret_cast<void*>(i + 1);
  lci::comp_signal_x(comp, status).call();
}

uint64_t my_cq_pop(lci::comp_t comp)
{
  lci::error_t error(lci::errorcode_t::retry);
  lci::status_t status;
  while (!error.is_ok()) std::tie(error, status) = lci::cq_pop(comp);
  return reinterpret_cast<uint64_t>(status.ctx) - 1;
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
  lci::free_cq_x(&comp).call();
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
  lci::global_initialize();
  const int nthreads = 16;
  const int n = 100000;
  ASSERT_EQ(n % nthreads, 0);
  const int n_per_thread = n / nthreads;
  bool flags[n];
  memset(flags, 0, sizeof(flags));
  lci::comp_t comp = lci::alloc_cq();
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
  lci::global_finalize();
}

}  // namespace test_cq