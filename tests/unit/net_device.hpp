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
  fprintf(stderr, "Starting loopback test\n");
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
    error.assert_no_fatal();
  }
  retry_count = 0;
  error.reset();
  while (!error.is_ok()) {
    if (++retry_count > retry_max) {
      throw std::runtime_error("Too many retries when posting send");
    }
    lcixx::net_post_send_x(0, address, size, mr, &error).call();
    error.assert_no_fatal();
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
      fprintf(stderr, "Got %lu statuses\n", tmp.size());
      continue;
    }
    statuses.insert(statuses.end(), tmp.begin(), tmp.end());
  }
  lcixx::deregister_memory_x(&mr).call();
  lcixx::g_runtime_fina_x().call();
}