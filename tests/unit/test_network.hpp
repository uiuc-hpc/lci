namespace test_network
{
TEST(NETWORK, reg_mem)
{
  lcixx::g_runtime_init_x().call();
  const int size = 1024;
  void* address = malloc(size);
  lcixx::mr_t mr;
  lcixx::register_memory_x(address, size, &mr).call();
  lcixx::deregister_memory_x(&mr).call();
  lcixx::g_runtime_fina_x().call();
}

TEST(NETWORK, poll_cq)
{
  lcixx::g_runtime_init_x().call();
  std::vector<lcixx::net_status_t> statuses;
  lcixx::net_poll_cq_x(&statuses).call();
  ASSERT_EQ(statuses.size(), 0);
  lcixx::g_runtime_fina_x().call();
}

TEST(NETWORK, loopback)
{
  lcixx::g_runtime_init_x().call();
  const int size = 1024;
  void* address = malloc(size);
  lcixx::mr_t mr;
  lcixx::register_memory_x(address, size, &mr).call();
  lcixx::error_t error;
  const int retry_max = 1000000;
  int retry_count = 0;
  while (!error.is_ok()) {
    if (++retry_count > retry_max) {
      throw std::runtime_error("Too many retries when posting receive");
    }
    lcixx::net_post_recv_x(address, size, mr, &error).call();
  }
  retry_count = 0;
  error.reset();
  while (!error.is_ok()) {
    if (++retry_count > retry_max) {
      throw std::runtime_error("Too many retries when posting send");
    }
    lcixx::net_post_send_x(0, address, size, mr, &error).call();
  }
  std::vector<lcixx::net_status_t> statuses;
  retry_count = 0;
  while (statuses.size() < 2) {
    if (++retry_count > retry_max) {
      throw std::runtime_error("Too many retries when polling cq");
    }
    std::vector<lcixx::net_status_t> tmp;
    lcixx::net_poll_cq_x(&tmp).call();
    if (!tmp.empty()) {
      statuses.insert(statuses.end(), tmp.begin(), tmp.end());
    }
  }
  lcixx::deregister_memory_x(&mr).call();
  lcixx::g_runtime_fina_x().call();
}

void test_loopback_mt(int id, int nmsgs, int size, void* address,
                      lcixx::mr_t mr)
{
  lcixx::error_t error;
  for (int i = 0; i < nmsgs; ++i) {
    error.reset();
    while (!error.is_ok()) {
      lcixx::net_post_recv_x(address, size, mr, &error).call();
    }
    error.reset();
    while (!error.is_ok()) {
      lcixx::net_post_send_x(0, address, size, mr, &error).call();
    }
    std::vector<lcixx::net_status_t> statuses;
    while (statuses.size() < 2) {
      std::vector<lcixx::net_status_t> tmp;
      lcixx::net_poll_cq_x(&tmp).call();
      if (!tmp.empty()) {
        statuses.insert(statuses.end(), tmp.begin(), tmp.end());
      }
    }
  }
}

TEST(NETWORK, loopback_mt)
{
  lcixx::g_runtime_init_x().use_default_packet_pool(false).call();
  const int size = 1024;
  const int nthreads = 16;
  const int nmsgs = 10000;
  ASSERT_EQ(nmsgs % nthreads, 0);
  void* address = malloc(size);
  lcixx::mr_t mr;
  lcixx::register_memory_x(address, size, &mr).call();
  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_loopback_mt, i, nmsgs / nthreads, size, address, mr);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }
  lcixx::deregister_memory_x(&mr).call();
  lcixx::g_runtime_fina_x().call();
}

}  // namespace test_network