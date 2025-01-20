namespace test_cq
{
void cq_push(lcixx::comp_t comp, uint64_t i)
{
  lcixx::status_t status;
  status.ctx = reinterpret_cast<void*>(i + 1);
  lcixx::comp_signal_x(comp, status).call();
}

uint64_t cq_pop(lcixx::comp_t comp)
{
  lcixx::error_t error(lcixx::errorcode_t::retry);
  lcixx::status_t status;
  while (!error.is_ok()) lcixx::cq_pop_x(comp, &error, &status).call();
  return reinterpret_cast<uint64_t>(status.ctx) - 1;
}

TEST(CQ, singlethread)
{
  const int n = 100;
  lcixx::g_runtime_init_x().call();
  lcixx::comp_t comp;
  lcixx::alloc_cq_x(&comp).call();
  for (uint64_t i = 0; i < n; i++) {
    cq_push(comp, i);
  }
  for (uint64_t i = 0; i < n; i++) {
    ASSERT_EQ(cq_pop(comp), i);
  }
  lcixx::free_cq_x(&comp).call();
  lcixx::g_runtime_fina_x().call();
}

// all threads put and get
void test_multithread0(lcixx::comp_t cq, int start, int n, bool flags[])
{
  for (uint64_t i = 0; i < n; i++) {
    cq_push(cq, start + i);
  }
  for (int i = 0; i < n; i++) {
    uint64_t idx = cq_pop(cq);
    ASSERT_EQ(flags[idx], false);
    flags[idx] = true;
  }
}

TEST(CQ, multithread0)
{
  lcixx::global_initialize();
  const int nthreads = 16;
  const int n = 100000;
  ASSERT_EQ(n % nthreads, 0);
  const int n_per_thread = n / nthreads;
  bool flags[n];
  memset(flags, 0, sizeof(flags));
  lcixx::comp_t comp;
  lcixx::alloc_cq_x(&comp).call();
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
  lcixx::global_finalize();
}

}  // namespace test_cq