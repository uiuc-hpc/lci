namespace test_network
{
TEST(NETWORK, reg_mem)
{
  lcixx::g_runtime_init();
  const int size = 1024;
  void* address = malloc(size);
  lcixx::mr_t mr = lcixx::register_memory(address, size);
  lcixx::deregister_memory(&mr);
  lcixx::g_runtime_fina();
}

TEST(NETWORK, poll_cq)
{
  lcixx::g_runtime_init();
  auto statuses = lcixx::net_poll_cq();
  ASSERT_EQ(statuses.size(), 0);
  lcixx::g_runtime_fina();
}

TEST(NETWORK, loopback)
{
  lcixx::g_runtime_init();
  const int size = 1024;
  void* address = malloc(size);
  lcixx::mr_t mr = lcixx::register_memory(address, size);
  while (lcixx::net_post_recv(address, size, mr).is_retry())
    ;
  while (lcixx::net_post_send(0, address, size, mr).is_retry())
    ;
  std::vector<lcixx::net_status_t> statuses;
  while (statuses.size() < 2) {
    auto tmp = lcixx::net_poll_cq();
    if (!tmp.empty()) {
      statuses.insert(statuses.end(), tmp.begin(), tmp.end());
    }
  }
  lcixx::deregister_memory(&mr);
  lcixx::g_runtime_fina();
}

void test_loopback_mt(int id, int nmsgs, int size, void* address,
                      lcixx::mr_t mr)
{
  for (int i = 0; i < nmsgs; ++i) {
    std::atomic<int> count(0);
    while (lcixx::net_post_recv_x(address, size, mr).ctx(&count)().is_retry())
      ;
    while (
        lcixx::net_post_send_x(0, address, size, mr).ctx(&count)().is_retry())
      ;
    std::vector<lcixx::net_status_t> statuses;
    while (count.load() < 2) {
      auto statuses = lcixx::net_poll_cq();
      for (auto& status : statuses) {
        auto* p = (std::atomic<int>*)status.ctx;
        p->fetch_add(1);
      }
    }
    ASSERT_EQ(count.load(), 2);
  }
}

TEST(NETWORK, loopback_mt)
{
  lcixx::g_runtime_init_x().runtime_mode(lcixx::attr_runtime_mode_t::network)();
  const int size = 1024;
  const int nthreads = 16;
  const int nmsgs = 10000;
  ASSERT_EQ(nmsgs % nthreads, 0);
  void* address = malloc(size);
  lcixx::mr_t mr = lcixx::register_memory(address, size);
  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_loopback_mt, i, nmsgs / nthreads, size, address, mr);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }
  lcixx::deregister_memory(&mr);
  lcixx::g_runtime_fina();
}

}  // namespace test_network